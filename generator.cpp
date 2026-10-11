// generator.cpp - random, undefined-behaviour-free C program generator.
//
// Usage:  ./gen SEED > prog.c
//
// Every program only uses uint32_t arithmetic (wraps by definition in C),
// shifts by constants 0..31, divisions whose divisor is forced non-zero, and
// array indices masked into range. So the program has exactly one correct
// output, and any disagreement between compilers is a real miscompile.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <string>
#include <vector>

// ---------- A tiny expression tree (AST) ----------

struct Expr {
    virtual ~Expr() = default;
    virtual std::string emit() const = 0;   // turn the node into C source text
};
using ExprPtr = std::unique_ptr<Expr>;

struct Leaf : Expr {                        // a variable name or a constant like 123u
    std::string text;
    explicit Leaf(std::string t) : text(std::move(t)) {}
    std::string emit() const override { return text; }
};

struct Bin : Expr {                         // (l op r) for + - * ^ & |
    std::string op;
    ExprPtr l, r;
    Bin(std::string o, ExprPtr a, ExprPtr b)
        : op(std::move(o)), l(std::move(a)), r(std::move(b)) {}
    std::string emit() const override {
        return "(" + l->emit() + " " + op + " " + r->emit() + ")";
    }
};

struct Shift : Expr {                       // (e << k) or (e >> k), k in 0..31 -> always defined
    bool left;
    int amount;
    ExprPtr e;
    Shift(bool lft, int amt, ExprPtr x) : left(lft), amount(amt), e(std::move(x)) {}
    std::string emit() const override {
        return "(" + e->emit() + (left ? " << " : " >> ") + std::to_string(amount) + "u)";
    }
};

struct SafeDivMod : Expr {                  // (l / (r | 1u)) -> divisor can never be zero
    bool mod;
    ExprPtr l, r;
    SafeDivMod(bool m, ExprPtr a, ExprPtr b) : mod(m), l(std::move(a)), r(std::move(b)) {}
    std::string emit() const override {
        return "(" + l->emit() + (mod ? " % " : " / ") + "(" + r->emit() + " | 1u))";
    }
};

struct ArrayRead : Expr {                   // a[(idx) & 7u] -> index always in bounds
    ExprPtr idx;
    explicit ArrayRead(ExprPtr i) : idx(std::move(i)) {}
    std::string emit() const override { return "a[(" + idx->emit() + ") & 7u]"; }
};

// ---------- Random program generator ----------

class Generator {
    std::mt19937 rng_;                      // seeded -> same seed, same program
    std::vector<std::string> vars_;

    // Our own range mapping (std::uniform_int_distribution differs between
    // standard libraries; mt19937 itself is fully specified by the standard).
    int rnd(int lo, int hi) { return lo + static_cast<int>(rng_() % static_cast<uint32_t>(hi - lo + 1)); }

    ExprPtr expr(int depth) {
        if (depth == 0 || rnd(0, 9) < 3) {
            if (rnd(0, 1))
                return std::make_unique<Leaf>(vars_[rnd(0, static_cast<int>(vars_.size()) - 1)]);
            return std::make_unique<Leaf>(std::to_string(rnd(0, 100000)) + "u");
        }
        // IMPORTANT: C++ does not define the order in which function arguments
        // are evaluated, and every rnd()/expr() call consumes random numbers.
        // So each draw gets its own statement; otherwise g++ and clang++ build
        // generators that print DIFFERENT programs for the same seed.
        switch (rnd(0, 9)) {
            case 0:
            case 1: {
                bool left = (rnd(0, 1) == 0);
                int amount = rnd(0, 31);
                ExprPtr operand = expr(depth - 1);
                return std::make_unique<Shift>(left, amount, std::move(operand));
            }
            case 2: {
                bool mod = (rnd(0, 1) == 1);
                ExprPtr l = expr(depth - 1);
                ExprPtr r = expr(depth - 1);
                return std::make_unique<SafeDivMod>(mod, std::move(l), std::move(r));
            }
            case 3: {
                ExprPtr idx = expr(depth - 1);
                return std::make_unique<ArrayRead>(std::move(idx));
            }
            default: {
                static const char* ops[] = {"+", "-", "*", "^", "&", "|"};
                const char* op = ops[rnd(0, 5)];
                ExprPtr l = expr(depth - 1);
                ExprPtr r = expr(depth - 1);
                return std::make_unique<Bin>(op, std::move(l), std::move(r));
            }
        }
    }

    std::string var() { return "v" + std::to_string(rnd(0, 5)); }

public:
    explicit Generator(uint32_t seed) : rng_(seed) {
        for (int k = 0; k < 6; ++k) vars_.push_back("v" + std::to_string(k));
        vars_.push_back("i");               // loop counter is usable inside expressions
    }

    std::string program() {
        std::string s = "#include <stdio.h>\n#include <stdint.h>\n\nint main(void) {\n";
        for (int k = 0; k < 6; ++k)
            s += "    uint32_t v" + std::to_string(k) + " = " + std::to_string(rnd(0, 1000000)) + "u;\n";
        s += "    uint32_t a[8] = {";
        for (int k = 0; k < 8; ++k)
            s += std::to_string(rnd(0, 1000000)) + "u" + (k < 7 ? ", " : "");
        s += "};\n";

        s += "    for (uint32_t i = 0; i < " + std::to_string(rnd(10, 40)) + "u; i++) {\n";
        int stmts = rnd(6, 10);
        for (int n = 0; n < stmts; ++n) {
            switch (rnd(0, 2)) {
                case 0: {
                    std::string target = var();
                    std::string value = expr(3)->emit();
                    s += "        " + target + " = " + value + ";\n";
                    break;
                }
                case 1: {
                    std::string index = expr(2)->emit();
                    std::string value = expr(3)->emit();
                    s += "        a[(" + index + ") & 7u] = " + value + ";\n";
                    break;
                }
                default: {
                    std::string lhs = expr(2)->emit();
                    std::string rhs = expr(2)->emit();
                    std::string then_var = var();
                    std::string then_val = expr(3)->emit();
                    std::string else_var = var();
                    std::string else_val = expr(3)->emit();
                    s += "        if (" + lhs + " < " + rhs + ") { " + then_var + " = " + then_val +
                         "; } else { " + else_var + " = " + else_val + "; }\n";
                }
            }
        }
        s += "    }\n";

        s += "    uint32_t chk = 0u;\n";
        for (int k = 0; k < 6; ++k) s += "    chk = (chk * 31u) ^ v" + std::to_string(k) + ";\n";
        for (int k = 0; k < 8; ++k) s += "    chk = (chk * 31u) ^ a[" + std::to_string(k) + "];\n";
        s += "    printf(\"%u\\n\", (unsigned)chk);\n    return 0;\n}\n";
        return s;
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s SEED\n", argv[0]);
        return 2;
    }
    Generator g(static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10)));
    std::fputs(g.program().c_str(), stdout);
    return 0;
}