# Design Approach

## Goal

The Precision Demotion Tool finds floating-point variables that can be represented with lower precision while keeping the final program output within a user-provided tolerance. It is designed as a source-to-source prototype: it analyzes C++ kernels, rewrites safe local `float` declarations, then verifies the transformed program by compiling and running both versions.

## High-Level Workflow

1. Parse the input C++ file with Clang.
2. Discover floating-point data flow from variable initializers, assignments, parameters, and return expressions.
3. Estimate a local numerical error for each discovered computation.
4. Propagate the output tolerance backward through dependencies.
5. Select a precision type for each variable.
6. Rewrite eligible local variables to the selected lower-precision type.
7. Compile and run the original and demoted programs.
8. Compare numeric outputs and report accuracy, runtime, speedup, and generated artifacts.

## Analysis Model

The tool treats each relevant floating-point variable as a flow node. A node records:

- The variable name.
- The right-hand-side expression that defines or updates it.
- Variables used by that expression.
- A local error estimate.
- The required tolerance propagated back from the output.
- The precision selected for that node.
- Whether the node is safe to rewrite.

The current model focuses on direct, verifier-friendly kernels where the output is observable through a numeric return or printed result. It is intentionally conservative and simple enough to explain, inspect, and extend.

## Dependency Tracking

Dependencies are collected from expressions by visiting declaration references. If an expression uses another floating-point variable, pointer to floating-point data, or floating-point array, the referenced name becomes a dependency of the current node.

For example:

```cpp
float c = a * b + 1.0f;
return c;
```

The tool records `c` as depending on `a` and `b`, and the return expression as depending on `c`. This allows the output tolerance to be distributed backward from `return c` to the variables that influenced it.

## Error Estimation

The current implementation uses a lightweight heuristic rather than a full formal floating-point error proof. It counts operations in an expression:

- Arithmetic binary operators add error weight.
- Floating-point unary operations add error weight.
- Floating-point-returning calls add extra operation weight.
- Floating-point literals add a smaller error weight.

This produces a local error estimate for each node. The estimate is used for demotion decisions, but final correctness is decided by verification: the original and demoted programs must produce outputs whose absolute difference is within tolerance.

## Backward Tolerance Propagation

The user supplies an output tolerance, such as `0.01`. The tool starts from the return/output dependencies and assigns each dependency a portion of that tolerance. It then walks flow nodes in reverse order:

1. Read the required tolerance for the current variable.
2. Choose a precision based on local error and required tolerance.
3. Subtract local error from the remaining budget.
4. Split the remaining budget across dependencies.
5. Keep the stricter requirement when multiple downstream values depend on the same variable.

This makes demotion decisions output-aware: variables close to the returned result, or variables with high local error, are kept at higher precision more often.

## Precision Selection

The precision selector maps local error and required tolerance to one of:

- `__fp16`
- `__bf16`
- `float`
- `double`

The tolerance-aware selector keeps a safety margin:

- `__fp16` when local error is at most 25% of the required tolerance.
- `__bf16` when local error is at most 50% of the required tolerance.
- `float` when local error is within the full required tolerance.
- `double` when local error exceeds the available requirement.

This is conservative enough for a prototype while still allowing visible demotion in simple kernels.

## Rewrite Policy

Only local `float` variable declarations are rewritten. Function parameters are analyzed but not rewritten because the current Clang target rejects `__fp16` parameter types in this setup. This avoids producing transformed code that fails before verification.

The rewrite step changes the source type token only. For example:

```cpp
float x = a + b;
```

may become:

```cpp
__fp16 x = a + b;
```

Variables that remain `float` are left unchanged.

## Verification Strategy

The verifier is the final correctness gate. It:

1. Runs the analyzer and rewriter.
2. Copies the original source to a temporary job directory.
3. Compiles the original source with `clang++`.
4. Compiles the demoted source with `clang++`.
5. Runs both executables.
6. Reads numeric output from both runs.
7. Computes absolute accuracy loss.
8. Passes only when accuracy loss is within the requested tolerance.

The verifier also records runtime, compile time, speedup, performance gain, CSV rows, optional JSON, and optional AST DOT output.

## Web Application Role

The browser UI is a thin workflow layer over the native tools. It lets users select or edit kernels, choose a tolerance, run verification, stream logs, and inspect generated artifacts. The backend does not reimplement the analysis; it invokes the compiled `verify` executable and serves the result files.

