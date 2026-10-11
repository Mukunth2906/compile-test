import os
import shutil
import subprocess
import sys


mingw_bin = r"C:\mingw64\bin"
portable_mingw_bin = r"D:\mingw64\bin"
for candidate in (mingw_bin, portable_mingw_bin):
    if os.path.isdir(candidate):
        os.environ["PATH"] = candidate + os.pathsep + os.environ.get("PATH", "")


def find_compiler(candidates):
    for name in candidates:
        resolved = shutil.which(name)
        if resolved:
            return resolved

    search_roots = [
        os.environ.get("MINGW_BIN"),
        mingw_bin,
        portable_mingw_bin,
        r"D:\mingw64\bin",
        r"C:\MinGW\bin",
        r"C:\msys64\mingw64\bin",
        r"C:\msys64\usr\bin",
        os.path.join(os.environ.get("USERPROFILE", ""), "mingw64", "bin"),
        os.path.join(os.environ.get("USERPROFILE", ""), "msys64", "mingw64", "bin"),
    ]

    for root in search_roots:
        if not root:
            continue
        for name in candidates:
            candidate = os.path.join(root, name)
            if not candidate.lower().endswith(".exe") and os.name == "nt":
                candidate += ".exe"
            if os.path.isfile(candidate):
                return candidate
    return None


cpp = find_compiler(["g++.exe", "g++", "clang++.exe", "clang++"])
c = find_compiler(["gcc.exe", "gcc", "clang.exe", "clang"])

if not cpp or not c:
    print(
        "No C/C++ compiler found. Install MinGW-w64 or MSYS2 and make sure its bin directory is on PATH.\n"
        "Typical locations: C:\\mingw64\\bin, C:\\MinGW\\bin, C:\\msys64\\mingw64\\bin",
        file=sys.stderr,
    )
    raise SystemExit(1)

# 1. Compile the C++ exam-writer program (creates gen.exe)
subprocess.run([cpp, "generator.cpp", "-o", "gen.exe"], check=True)

# 2. Command the exam-writer to create a random C program (seed '42')
with open("test.c", "w", encoding="utf-8") as f:
    subprocess.run(["gen.exe", "42"], stdout=f, check=True)

# 3. Hand the exam to the C compiler (GCC) with two different optimization levels
subprocess.run([c, "-O0", "test.c", "-o", "prog_O0.exe"], check=True)
subprocess.run([c, "-O3", "test.c", "-o", "prog_O3.exe"], check=True)

# 4. Run both compiled programs and record their answers
out_O0 = subprocess.run(["prog_O0.exe"], capture_output=True, text=True, check=True).stdout
out_O3 = subprocess.run(["prog_O3.exe"], capture_output=True, text=True, check=True).stdout

# 5. Grade the results
if out_O0 == out_O3:
    print(f"Pass: Both optimizations produced {out_O0.strip()}")
else:
    print(f"FAIL! Compiler Bug.\nO0: {out_O0}\nO3: {out_O3}")