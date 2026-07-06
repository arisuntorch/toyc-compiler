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

The optional `-opt` argument is accepted by the judge command line contract but
is currently ignored by this implementation.

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
- Constant expression folding plus simple immediate-form code generation for
  common add, subtract, equality, and comparison expressions.

## Notes

The generated assembly follows a simple stack-frame based convention. Local
variables and parameters are stored in each function's frame. Expression results
are passed through `a0`; function arguments use `a0` to `a7` first and then the
caller stack.
