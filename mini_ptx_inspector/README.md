# Mini PTX Inspector

A small C++17 command-line static analyzer for a readable subset of NVIDIA PTX assembly.

## Features
- Counts common instruction categories and prints an opcode summary.
- Removes `//` and `/* ... */` comments before analysis.
- Collects labels and validates direct `bra` / `bra.uni` targets.
- Reports missing branch targets with a line number.
- Uses exit code `0` for valid branch targets, `1` for validation errors, and `2` for usage/file errors.

## Build (Windows with MinGW g++)
From this folder:

```powershell
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o ptx_inspector.exe
```

## Run

```powershell
.\ptx_inspector.exe .\samples\sample.ptx
.\ptx_inspector.exe .\samples\bad_branch.ptx
```

The second command should report `MISSING_LABEL` and return exit code 1.

## Tests

Requires Python 3 and a working `g++` compiler:

```powershell
python .\tests\test_ptx_inspector.py
```

The test script compiles the program and checks both a valid file and an invalid branch target.

## How it works
1. Read the PTX source and remove line/block comments.
2. Find labels such as `DONE:`.
3. Split statements at semicolons and identify each instruction's opcode.
4. For direct branches, save the target label.
5. After parsing, check each saved target against the label set.
6. Print instruction statistics and validation errors.

## Scope and limitations
This is an educational static analyzer, not a complete PTX parser or NVIDIA compiler. It handles common, semicolon-terminated instructions and direct `bra` / `bra.uni` targets. It does not execute PTX, validate instruction semantics, support indirect branches such as `brx`, or require an NVIDIA GPU. The sample PTX is a small illustrative input, not a claim of compiler-generated output.
