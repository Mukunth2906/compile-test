# Compiler Differential Test Harness

This project checks whether two optimization levels of the same compiler produce the same result for a generated C program.

## What it does

- `generator.cpp` emits a random C program designed to avoid undefined behavior.
- `driver.py`:
  1. compiles the generator with `g++`
  2. runs it with a fixed seed to produce a random C file
  3. compiles that file twice with GCC: `-O0` and `-O3`
  4. runs both executables
  5. compares their output and prints `Pass` or `FAIL! Compiler Bug.`

The idea is to detect optimizer regressions or miscompilations by generating many deterministic programs and checking that the optimized and unoptimized outputs match.

## Files

- `generator.cpp` — random C program generator
- `driver.py` — orchestration script for build and comparison
- `test.c` — generated candidate program (created at runtime)
- `prog_O0.exe` / `prog_O3.exe` — compiled test programs (generated at runtime)

## Requirements

You need a working C/C++ toolchain installed on your machine.

On Windows, MinGW-w64 is commonly used. The script checks typical MinGW paths such as:

- `D:\mingw64\bin`
- `C:\mingw64\bin`
- `C:\MinGW\bin`
- `C:\msys64\mingw64\bin`

Make sure `gcc.exe` and `g++.exe` are available on PATH.

## Run it

From PowerShell:

```powershell
$env:Path = "D:\mingw64\bin;" + $env:Path
cd D:\compiler-test
python driver.py
```

## Example output

```text
Pass: Both optimizations produced 862527539
```

If the outputs differ, the script prints:

```text
FAIL! Compiler Bug.
O0: ...
O3: ...
```

## Notes

This is a small differential testing harness for compiler correctness, not a general-purpose build system.
