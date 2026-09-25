"""Run verifier regressions against a source-built compiler, without linking."""
import argparse
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = Path(__file__).resolve().parent
PASS_CASES = [
    "loop_symbolic_bounded_pass", "loop_allocations_pass", "branch_fields_pass",
    "loop_values_pass", "for_values_pass", "loop_zero_iterations_pass",
    "loop_return_paths_pass", "loop_boundary_pass",
    "assignment_values_pass",
    "branch_values_pass",
    "nested_branch_values_pass",
    "early_return_guard_pass",
    "branch_free_pass",
    "shadowed_values_pass",
    "shadowed_pointer_pass",
    "branch_return_free_pass",
    "contract_pass", "business_logic_pass", "contract_shadowed_args_pass",
]
FAIL_CASES = [
    "loop_later_bounds_fail", "loop_zero_return_fail", "loop_repeated_free_fail",
    "assignment_bounds_fail",
    "repeated_assignment_fail",
    "branch_bounds_fail",
    "early_return_leak_fail",
    "branch_double_free_fail",
    "shadowed_bounds_fail",
    "shadowed_pointer_leak_fail",
    "cve_buffer_overflow", "cve_buffer_underread", "cve_loop_overflow",
    "cve_division_by_zero", "cve_use_after_free", "cve_double_free",
    "cve_memory_leak", "cve_use_after_move", "cve_invalid_borrow_free",
    "contract_fail_requires", "contract_fail_ensures", "business_logic_fail",
    "contract_shadowed_args_fail", "call_argument_bounds_fail",
    "array_assignment_rhs_fail", "independent_array_reads_fail",
    "independent_calls_fail", "contract_multiple_returns_fail",
]

INCONCLUSIVE_CASES = [
    "loop_budget_inconclusive", "loop_symbolic_inconclusive",
    "loop_nested_budget_inconclusive",
]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", type=Path, default=ROOT / "build" / "native" /
                        ("alu.exe" if sys.platform == "win32" else "alu"))
    parser.add_argument("--timeout", type=float, default=30)
    args = parser.parse_args()
    compiler = args.compiler.resolve()
    if not compiler.is_file():
        parser.error(f"Compiler not found: {compiler}. Run python scripts/build_native.py.")
    failures = []
    for name in PASS_CASES + FAIL_CASES + INCONCLUSIVE_CASES:
        try:
            result = subprocess.run(
                [str(compiler), "check", str(TEST_DIR / (name + ".alu")), "--std-path", str(ROOT)],
                cwd=ROOT, capture_output=True, text=True, timeout=args.timeout,
            )
        except subprocess.TimeoutExpired:
            failures.append(name)
            print(f"FAIL {name}: compiler timed out")
            continue
        output = result.stdout + result.stderr
        # A crash, parse error, or solver timeout is not evidence that the
        # expected safety obligation was rejected.
        if name in INCONCLUSIVE_CASES:
            passed = (result.returncode == 1 and
                      "Z3 Verification Inconclusive: loop proof budget exhausted" in output and
                      "Z3 Verification Passed" not in output)
        elif name in FAIL_CASES:
            passed = result.returncode == 1 and "Z3 Verification Failed" in output
        else:
            passed = result.returncode == 0 and "Z3 Verification Passed" in output
        print(f"{'PASS' if passed else 'FAIL'} {name}")
        if not passed:
            failures.append(name)
            print(output)
    count = len(PASS_CASES) + len(FAIL_CASES) + len(INCONCLUSIVE_CASES)
    print(f"{count - len(failures)}/{count} verifier regression cases passed.")
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
