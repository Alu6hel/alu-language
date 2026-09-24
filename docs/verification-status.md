# Verification status and development gates

ALU is under active development. Z3 currently checks selected obligations in an
abstract model. Successful compilation is not yet a proof of whole-program memory
safety, absence of leaks, or immunity to exploits. The C++ runtime, foreign calls,
LLVM lowering, and toolchain are part of the trusted computing base.

## Reproduce current checks

Use Python 3 and a C++17 Clang toolchain. On Linux install the Z3 development
package; Windows uses the repository's bundled Z3 headers and import library.

```text
python scripts/build_native.py --target alu
python tests/z3_tests/run_z3_suite.py
python tests/native/run_codegen_suite.py
build/native/alu check path/to/program.alu --std-path .
```

Use alu.exe on Windows. The verifier tests check expected acceptance and rejection;
the LLVM tests assemble generated IR independently of runtime linking. These are
regression checks, not an exhaustive proof. The source-build script also accepts
--target alupm, --target alu_bindgen, and --z3-root.

## Current corrections

- Contract return expressions bind to the symbol read by postconditions.
- Actual call arguments are evaluated before formal parameter substitution.
- Array accesses inside call arguments and assignment values are checked.
- Independent unknown reads and calls do not share a single symbolic value.
- Each verifier worker owns its allocation counter; allocation does not race
  across parallel routines.
- Solver unknown/timeout outcomes stop verification instead of being accepted.
- Concrete structs are not also emitted as opaque LLVM types; ptr/managed struct
  wrappers agree with the pointer returned by struct allocation.

## Remaining requirements for the language goal

1. Specify integer overflow, division, pointer ownership, aliasing, lifetimes,
   exceptions, concurrency, and FFI semantics; align proofs with native behavior.
2. Model control flow soundly: unique assignment versions, lexical scope and
   branch merges, early returns, loop invariants or sound loop analysis.
3. Complete coverage across namespaces, templates, casts, unsafe blocks, method
   calls, exceptions, all pointer syntaxes, and all AST node types. Unsupported
   proof obligations must produce an explicit diagnostic.
4. Prove ownership transfer and release behavior through the generated runtime,
   including managed fields, aggregates, exceptions, and concurrency. Validate
   with sanitizers, adversarial programs, and differential tests.
5. Provide reviewable proof evidence/certificates tied to exact source, compiler,
   verifier version, assumptions, and obligations. Independently check evidence.
6. Deliver consistent source builds, installable toolchains, portable runtime
   linking, alupm init/build/run, formatter, linter, LSP, and documented syntax.
7. Provide stable machine-readable diagnostics and validation commands for AI
   tools, reproducible examples, and deterministic regression/fuzz testing.
8. Gate releases on those guarantees across supported platforms. Audit public
   security claims against the proven scope before claiming completion.

The full language goal remains open while any gate above is incomplete.
