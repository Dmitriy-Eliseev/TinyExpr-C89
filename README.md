# TinyExpr-C89 (Adaptation)

This is a C89-compatible adaptation of the [original TinyExpr library](https://github.com/codeplea/tinyexpr) by Lewis Van Winkle. TinyExpr is a tiny recursive descent parser and evaluation engine for mathematical expressions in C.

The main goal of this adaptation is to make the library usable with old C89 (ANSI C 1989) compilers on DOS systems, such as Turbo C, Borland C++, Open Watcom, or DJGPP. It removes C99-specific features while preserving the original functionality, performance, and API.

**Important**: This is an altered version and not the original software. All changes are distributed under the same Zlib license as the original.

## Features

- Parses and evaluates mathematical expressions (e.g., "2+3*4", "sin(pi/2)", "pow(2,3)").
- Supports variables, built-in functions (sin, cos, pow, etc.), and custom functions.
- Lightweight: No external dependencies beyond standard C libraries.
- **C89 Adaptations**:
  - Removed C99 macros (e.g., those using `__VA_ARGS__` and compound literals).
  - Replaced with direct function calls and fixed-size arrays.
  - Moved variable declarations to the beginning of blocks.
  - Added manual NULL checks and memory management for compatibility with old compilers.
  - No changes to the core parsing or evaluation logic — fully equivalent to the original.

For a full list of changes, see the header comments in `tinyexpr.c`.

## Installation and Compilation

### Requirements
- A C89-compatible compiler (e.g., Turbo C 2.0, DJGPP, Open Watcom, or modern GCC/Clang with `-std=c89` flag).
- Standard C libraries: `<stdlib.h>`, `<math.h>`, `<string.h>`, `<stdio.h>`, `<ctype.h>`, `<limits.h>`.

### Building
1. Clone the repository:
   ```
   git clone https://github.com/Dmitriy-Eliseev/TinyExpr-C89.git
   cd TinyExpr-C89
   ```

2. Compile
### On Modern Systems (GCC/Clang):
   ```
   gcc -std=c89 -O3 -o example tinyexpr.c example.c -lm
   ```
### On DOS (Open Watcom):
   ```
   wcl -3 -ox -s -fe=EXAMPLE.EXE EXAMPLE.C TINYEXPR.C
   ```

## Usage
Include `tinyexpr.h` in your code and link `tinyexpr.c`.

Available functions:
- `double te_interp(const char *expression, int *error);`
- `te_expr* te_compile(const char *expression, const te_variable *vars, int var_count, int *error);`
- `double te_eval(const te_expr *n);`
- `void te_free(te_expr *n);`

### Example
```c
#include "tinyexpr.h"
#include <stdio.h>

int main() {
 const char *expression = "2 + 3 * sin(pi/2)";
 int error;
 double result = te_interp(expression, &error);

 if (error == 0) {
     printf("Result: %f\n", result);  // Output: Result: 5.000000
 } else {
     printf("Error at position %d\n", error);
 }

 return 0;
}
```
For more advanced usage (variables, custom functions), see the [original documentation](https://github.com/codeplea/tinyexpr#usage).

## License
This adaptation is licensed under the Zlib license, same as the original. See [LICENSE](LICENSE) for full license text.

- Original copyright: Copyright (c) 2015-2020 Lewis Van Winkle.
- Adaptation copyright: Copyright (c) 2025 Dmitriy Eliseev (for C89 changes only).

You are free to use, modify, and distribute this software, as long as you follow the Zlib terms (e.g., mark altered versions clearly and keep the notice).

## Acknowledgments
- Huge thanks to Lewis Van Winkle for the original TinyExpr library. This adaptation would not exist without it!
- Original repository: [codeplea/tinyexpr](https://github.com/codeplea/tinyexpr).
