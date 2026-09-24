# ALU: a systems language with compile-time verification

ALU aims to combine approachable syntax, native performance, automatic memory
management, and mathematical checks at compile time. The native C++ compiler
parses ALU, runs semantic analysis and Z3 checks, emits LLVM IR, and invokes Clang.

The language and toolchain are under active development. Current Z3 checks cover
selected obligations in an abstract model. They do **not yet establish absolute
memory safety, zero memory leaks, or immunity to exploits**. See
[verification status and remaining gates](docs/verification-status.md) for the
scope, trusted components, and work required for those guarantees.

## Build from source

Requirements: Python 3, a C++17 Clang toolchain, and Z3 headers/libraries.
On Linux install `clang` and `libz3-dev`; Windows builds use the bundled Z3 SDK.

```sh
python scripts/build_native.py --target alu
python tests/z3_tests/run_z3_suite.py
python tests/native/run_codegen_suite.py
```

The compiler is written to `build/native/alu` (`alu.exe` on Windows). To build
the package manager and binding generator too, omit `--target alu`.
Use `--cxx`, `--output-dir`, and `--z3-root` to select your toolchain paths.

## Check a program

```text
routine main() -> int {
    int values[3];
    values[0] = 42;
    return 0;
}
```

```sh
build/native/alu check example.alu --std-path .
build/native/alu --help
```

`check` parses, analyzes, and runs the verifier without generating or linking
files. It returns zero when the current checks pass and nonzero on failure.
Solver timeouts or unknown outcomes fail verification. For native code generation,
use `alu build example.alu`; runtime linking currently requires further platform
work. LLVM assembly tests are independent of that runtime.

## Development direction

- Sound proofs that match native integer, memory, control-flow, and FFI semantics.
- An approachable ownership model without user-written lifetime annotations.
- Reliable `alupm init`, `alupm build`, and `alupm run` workflows.
- A formatter, linter, editor support, and stable diagnostics for AI development.
- Reproducible toolchains, regression tests, fuzzing, and independently checkable
  proof evidence before stronger security claims.

ALU includes standard-library and experimental mobile, graphics, networking, and
security integrations. Their presence in the repository does not imply that all
targets are release-ready. The existing [bounty terms](BOUNTY.md) describe the
conditional bounty program; they are not a verification certificate.

See [verification status](docs/verification-status.md) for the current engineering
gates and test commands.

