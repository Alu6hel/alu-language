"""Validate generated LLVM using Clang, independently of platform runtime linking."""
import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CASES = {
    "struct_allocation": "struct Node { int value; } routine main() { ptr<Node> p = new Node; p.value = 7; }",
    "managed_struct": "struct Node { int value; } routine main() { managed<Node> p = new Node; p.value = 7; }",
    "forward_reference": "struct Parent { managed<Child> child; } struct Child { int value; } routine main() { }",
    "recursive_layout": "struct Node { ptr<Node> next; int value; } routine main() { }",
    "external_opaque": "extern routine lookup() -> Handle; routine main() { }",
    "primitive_pointer": "routine main() { ptr<int> p = new int; *p = 7; free(p); }",
    "nested_pointer": "routine passthrough(ptr<ptr<Node>> p) -> ptr<ptr<Node>> { return p; } struct Node { int value; } routine main() { }",
    "struct_arithmetic": (ROOT / "test_arithmetic.alu").read_text(),
    "right_shift": "routine main() -> int { return 8 >> 1; }",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--clang", default="clang")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="alu-codegen-") as directory:
        work = Path(directory)
        driver = work / ("driver.exe" if sys.platform == "win32" else "driver")
        sources = ["lexer.cpp", "parser.cpp", "llvm_codegen.cpp", "error_reporter.cpp"]
        subprocess.run([args.cxx, "-std=c++17", "-I", str(ROOT / "cpp_frontend"),
                        str(ROOT / "tests/native/codegen_driver.cpp"),
                        *[str(ROOT / "cpp_frontend" / s) for s in sources],
                        "-o", str(driver)], check=True, cwd=work)
        for name, source in CASES.items():
            emitted = subprocess.run([str(driver)], input=source, text=True,
                                     capture_output=True, timeout=30, cwd=work)
            if emitted.returncode:
                print(f"FAIL {name}\n{emitted.stderr}")
                return 1
            ir = work / (name + ".ll")
            ir.write_text(emitted.stdout)
            assembled = subprocess.run([args.clang, "-x", "ir", "-c", str(ir), "-o", str(work / (name + ".o"))],
                                       text=True, capture_output=True, timeout=30)
            if assembled.returncode:
                print(f"FAIL {name}\n{assembled.stderr}\n{emitted.stdout}")
                return 1
            print(f"PASS {name}")
    print(f"{len(CASES)}/{len(CASES)} LLVM assembly regressions passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
