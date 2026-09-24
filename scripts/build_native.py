"""Build the native toolchain using explicit sources; no checked-in binaries required."""
import argparse
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCES = {
    "alu": ["main", "lexer", "parser", "semantic_analyzer", "llvm_codegen", "linker", "error_reporter", "z3_verifier"],
    "alupm": ["alupm", "toml_parser", "semver", "dependency_resolver", "package_fetcher", "build_driver", "registry"],
    "alu_bindgen": ["alu_bindgen_main", "bindgen"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "build" / "native")
    parser.add_argument("--target", choices=[*SOURCES, "all"], default="all")
    parser.add_argument("--z3-root", type=Path)
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    z3_root = args.z3_root.resolve() if args.z3_root else (ROOT / "z3" if sys.platform == "win32" else None)
    for target in SOURCES if args.target == "all" else [args.target]:
        exe = output / (target + (".exe" if sys.platform == "win32" else ""))
        cmd = [args.cxx, "-std=c++17", "-O2"]
        cmd += [str(ROOT / "cpp_frontend" / (source + ".cpp")) for source in SOURCES[target]]
        if target == "alu":
            if z3_root:
                cmd += ["-I", str(z3_root / "include"), "-L", str(z3_root / "bin")]
            if sys.platform == "win32" and z3_root:
                cmd += [str(z3_root / "bin" / "libz3.lib")]
            else:
                cmd += ["-lz3"]
        if sys.platform != "win32":
            cmd += ["-pthread"]
        cmd += ["-o", str(exe)]
        print("Building", target, flush=True)
        subprocess.run(cmd, check=True, cwd=ROOT)
        if target == "alu" and sys.platform == "win32" and z3_root:
            shutil.copy2(z3_root / "bin" / "libz3.dll", output / "libz3.dll")
        print(exe, flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
