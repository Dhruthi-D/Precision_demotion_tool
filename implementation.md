# LLVM And Clang Implementation Details

## Native Tool Layout

The native implementation is split into two executables:

- `tool`: performs Clang AST analysis, precision propagation, source rewriting, and optional AST DOT export.
- `verify`: runs `tool`, compiles original and demoted sources, executes both programs, compares output, and writes reports.

The CMake target links the analyzer against Clang libraries:

- `clangTooling`
- `clangBasic`
- `clangASTMatchers`
- `clangRewrite`

The tool uses LLVM support utilities for command-line options, filesystem creation, and output streams.

## Command-Line Integration

The analyzer defines LLVM command-line options in `src/main.cpp`:

- `--tolerance=<value>` sets the allowed absolute output error budget.
- `--output=<path>` sets the rewritten source file path.
- `--ast-dot=<path>` optionally writes a Graphviz DOT expression AST.

`CommonOptionsParser` handles normal Clang tooling arguments, including the source file and compilation flags after `--`. `ClangTool` then runs a custom `ASTFrontendAction` over the requested translation unit.

Example:

```bash
./tool ../test/kernel1.cpp --tolerance=0.01 --output=../temp/demoted.cpp -- -std=c++17
```

## Frontend Action

`MyFrontendAction` derives from `clang::ASTFrontendAction`. Its main responsibilities are:

1. Initialize the global `clang::Rewriter` with the current `SourceManager` and language options.
2. Register AST matchers with a `MatchFinder`.
3. Return the matcher's AST consumer.
4. At the end of the file, run propagation, rewriting, DOT output, and final source emission.

The end-of-file hook is important because the complete flow graph must be collected before precision decisions can be made.

## AST Matchers

The tool registers four main matcher groups:

```cpp
varDecl(
    hasInitializer(expr()),
    unless(parmVarDecl()),
    isExpansionInMainFile()
).bind("varDecl")
```

Collects initialized variables in the main file. The callback filters this further to source-level `float` declarations.

```cpp
parmVarDecl(
    hasType(asString("float")),
    isExpansionInMainFile()
).bind("paramDecl")
```

Collects `float` function parameters. These are represented as flow nodes but marked as not rewriteable.

```cpp
binaryOperator(
    isAssignmentOperator(),
    isExpansionInMainFile()
).bind("assign")
```

Collects assignment flows and merges right-hand-side dependencies and local error into the existing flow node for the assigned variable.

```cpp
returnStmt(
    isExpansionInMainFile()
).bind("ret")
```

Collects return dependencies and return-expression error so tolerance propagation can start near the observable output.

All matchers use `isExpansionInMainFile()` to avoid rewriting included headers or library code.

## FlowNode Representation

The analyzer stores each discovered value in a `FlowNode`:

```cpp
struct FlowNode {
    std::string name;
    std::string rhs;
    SourceLocation typeLoc;
    double localError = 0.0;
    double requiredTolerance = 0.0;
    PrecisionType assignedType = FP32;
    bool rewriteAllowed = true;
    std::set<std::string> dependencies;
};
```

`typeLoc` is the source location of the `float` type token. The rewriter later uses it to replace exactly that token with `__fp16`, `__bf16`, `float`, or `double`.

## Expression Printing

Expressions are converted back to readable source text with:

```cpp
expr->printPretty(out, nullptr, PrintingPolicy(context.getLangOpts()));
```

This is used only for diagnostics and DOT labels. Rewrite operations use source locations instead of pretty-printed source.

## Dependency Visitor

`DependencyVisitor` derives from `RecursiveASTVisitor<DependencyVisitor>`. It walks expressions and gathers:

- Floating-point declaration references.
- Arithmetic operation count.
- Floating-point literal count.
- Floating-point-returning call count.

Important visitor methods include:

- `VisitDeclRefExpr`
- `VisitBinaryOperator`
- `VisitUnaryOperator`
- `VisitCallExpr`
- `VisitFloatingLiteral`

The visitor uses `QualType` checks to decide whether a referenced declaration is a floating-point carrier. Direct real floating types, pointers to real floating types, and arrays of real floating element type are included.

## Local Error Heuristic

`DependencyVisitor::estimateLocalError()` currently computes:

```cpp
arithmeticOps * 1e-4 + literals * 1e-5
```

Function calls returning floating-point values contribute extra arithmetic weight. Assignments add at least `1e-4` when merged into an existing node, so update flows affect precision choices even if the expression is simple.

## Backward Propagation

`propagateBackward()` computes required tolerances and assigned types after all flows are known.

The initial budget is based on the user tolerance minus return-expression error. It keeps at least half the output tolerance available:

```cpp
max(OutputTolerance - returnExpressionError, OutputTolerance * 0.5)
```

That budget is divided across return dependencies. Then the tool walks `flowNodes` in reverse order and:

1. Finds the current variable's required tolerance.
2. Calls `choosePrecision(localError, requiredTolerance)`.
3. Stores the result in `precisionTable`.
4. Computes a child budget from the remaining tolerance.
5. Divides that budget across dependencies.
6. Uses the minimum requirement when a dependency is reached through multiple paths.

The precision selection logic lives in `src/PrecisionPropagator.cpp` and returns values from `PrecisionType`.

## Source Rewriting

The rewrite step uses `clang::Rewriter::ReplaceText`:

```cpp
Rewrite.ReplaceText(node.typeLoc, 5, newType);
```

The length is `5` because eligible rewritten declarations are filtered to the token `float`. The replacement string comes from `precisionToString()`:

- `FP64` -> `double`
- `FP32` -> `float`
- `FP16` -> `__fp16`
- `BF16` -> `__bf16`

Nodes marked `rewriteAllowed = false` are skipped, and nodes assigned back to `float` are also skipped.

## Output Emission

At the end of source processing, the tool writes the edited main file buffer:

```cpp
Rewrite.getEditBuffer(
    Rewrite.getSourceMgr().getMainFileID()
).write(out);
```

The output path's parent directory is created with LLVM filesystem helpers before writing:

```cpp
llvm::sys::fs::create_directories(
    llvm::sys::path::parent_path(outPath));
```

The same edit buffer is also printed to standard output for inspection.

## AST DOT Export

When `--ast-dot` is provided, the tool builds a simplified expression tree in Graphviz DOT format. The exporter handles:

- Binary operators.
- Unary operators.
- Declaration references.
- Integer and floating literals.
- Calls.
- Generic expression children as a fallback.

Each captured initializer, assignment, and return expression is written as a DOT subgraph. The web backend can render this DOT file to SVG with Graphviz `dot -Tsvg`.

## Verification Executable

`src/Verifier.cpp` is intentionally separate from the Clang tool. It uses ordinary process execution to drive the full workflow:

1. Create a temporary directory under `../temp`.
2. Run `./tool` from `build/`.
3. Copy the original source into the temp directory.
4. Compile original and demoted sources with `clang++`.
5. Run both binaries and redirect output to files.
6. Parse numeric output as `double`.
7. Compare absolute difference against tolerance.
8. Append `results/evaluation.csv`.
9. Optionally write a JSON result file.

The verifier exits with `0` on pass and `1` on failure, so scripts and the web backend can treat verification failure as a failed job.

## Backend Integration

The Node backend does not call LLVM APIs directly. It invokes the compiled verifier:

```js
runProcess(job, "./verify", args, { cwd: buildDir });
```

For each run, it creates a job directory, writes the submitted C++ source, passes relative paths to `verify`, streams stdout and stderr through server-sent events, and serves generated files such as:

- Input source.
- Analysis log.
- Original copied source.
- Demoted source.
- AST DOT.
- AST SVG.
- CSV report.

This keeps LLVM-specific logic inside the native tool and leaves the web layer responsible only for orchestration and presentation.

#

