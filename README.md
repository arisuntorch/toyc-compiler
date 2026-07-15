# ToyC Compiler

This project is a C++20 compiler for the course ToyC language. It reads ToyC
source code from standard input and writes RISC-V32 assembly to standard output.

## Build

```sh
make
```

The build creates an executable named `main`.

## Use

```sh
./main < input.tc > output.s
```

The optional `-opt` argument is accepted for compatibility. Both modes run the
same analysis and optimized RISC-V32 code-generation pipeline; the compiler
does not execute the input program or replace `main` with a precomputed result.

## Implemented Language Coverage

- Global and local `int` variables.
- Global and local `const int` constants with compile-time constant folding.
- Functions returning `int` or `void`.
- Integer parameters, including calls with more than eight parameters.
- Nested block scopes and shadowing.
- `if`, `else`, `while`, `break`, `continue`, and `return`.
- Arithmetic, relational, equality, logical, unary, parenthesized, variable, and
  function-call expressions.
- Short-circuit code generation for `&&` and `||`.
- Constant expression folding plus immediate-form code generation for common
  arithmetic, equality, and comparison expressions.
- A function-level AST optimization pass with lexically scoped constant and
  copy propagation, common-subexpression elimination, loop-invariant code
  motion, conservative dead-code elimination, and constant control-flow
  simplification.
- Register-based expression evaluation for call-free expressions, avoiding most
  temporary stack traffic in loop bodies.
- Loop-depth-weighted allocation of hot variables and constants to `s1`-`s11`,
  reducing stack traffic and repeated immediate materialization.
- Direct conditional branches for `if` and `while` comparisons.
- Strength reduction for division and modulo by positive constants, plus
  branch-helper inlining on the RISC-V32 path.
- Tail-recursive self calls are rewritten into parameter updates plus a jump.

## Notes

The generated assembly follows a simple stack-frame based convention. Local
variables and parameters are stored in each function's frame. Expression results
are passed through `a0`; function arguments use `a0` to `a7` first and then the
caller stack.
