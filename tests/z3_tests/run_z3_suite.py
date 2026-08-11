import subprocess
import os
import sys

# Get absolute paths
test_dir = os.path.dirname(os.path.abspath(__file__))
project_root = os.path.abspath(os.path.join(test_dir, "..", ".."))

if os.name == 'nt':
    alu_cxx = os.path.join(project_root, "cpp_frontend", "alu.exe")
else:
    alu_cxx = os.path.join(project_root, "cpp_frontend", "alu_cxx")

# We expect the compiler to be built. Let's make sure it exists.
if not os.path.exists(alu_cxx):
    print(f"Error: Could not find alu compiler at {alu_cxx}")
    sys.exit(1)

# Test definitions: (filename, should_fail, should_have_z3_fatal)
# should_fail=True means we expect non-zero exit code
# should_have_z3_fatal=True means we expect "[ALU CXX Z3 FATAL]" in output
test_cases = [
    # Existing bounds-check tests (should fail with Z3 FATAL)
    ("cve_buffer_overflow.alu",      True,  True),
    ("cve_buffer_underread.alu",     True,  True),
    ("cve_loop_overflow.alu",        True,  True),
    ("cve_division_by_zero.alu",     True,  True),
    ("cve_use_after_free.alu",       True,  True),
    ("cve_double_free.alu",          True,  True),
    ("cve_memory_leak.alu",          True,  True),
    ("cve_use_after_move.alu",       True,  True),
    ("cve_invalid_borrow_free.alu",  True,  True),
    
    # Contract tests
    ("contract_pass.alu",            False, False),  # Should PASS (contracts satisfied)
    ("contract_fail_requires.alu",   True,  True),   # Should FAIL (@requires violated)
    ("contract_fail_ensures.alu",    True,  True),   # Should FAIL (@ensures violated)

    # Business Logic Asserts
    ("business_logic_pass.alu",      False, False),
    ("business_logic_fail.alu",      True,  True),
]

all_passed = True

print("=== Starting Z3 Theorem Prover CVE + Contract Test Suite ===")

for test_file, expect_fail, expect_z3_fatal in test_cases:
    file_path = os.path.join(test_dir, test_file)
    print(f"\nTesting: {test_file}")
    
    # Run the compiler from the project root so it can find std/ backend files
    cmd = [alu_cxx, "build", file_path]
    result = subprocess.run(cmd, cwd=project_root, capture_output=True, text=True)
    
    output = result.stdout + result.stderr
    
    failed = result.returncode != 0
    caught_by_z3 = "[ALU CXX Z3 FATAL]" in output
    
    if expect_fail:
        # We expect the compiler to fail with Z3 FATAL
        if failed and caught_by_z3:
            print(f"  [PASS] Z3 successfully caught the vulnerability and blocked compilation.")
        else:
            all_passed = False
            print(f"  [FAIL] Test did not meet expectations.")
            if not failed:
                print(f"    Compiler succeeded (Exit code 0), but should have failed!")
            if not caught_by_z3:
                print(f"    Z3 warning missing from output!")
            print("--- COMPILER OUTPUT ---")
            print(output)
            print("-----------------------")
    else:
        # We expect the compiler to succeed (no Z3 errors)
        if not failed and not caught_by_z3:
            print(f"  [PASS] Z3 verification passed as expected (contracts satisfied).")
        else:
            all_passed = False
            print(f"  [FAIL] Test did not meet expectations.")
            if failed:
                print(f"    Compiler failed (Exit code {result.returncode}), but should have succeeded!")
            if caught_by_z3:
                print(f"    Unexpected Z3 FATAL in output!")
            print("--- COMPILER OUTPUT ---")
            print(output)
            print("-----------------------")

print("\n================================================")
if all_passed:
    print("SUCCESS: All Z3 tests passed! Zero-Day Immunity + Contract Enforcement verified.")
    sys.exit(0)
else:
    print("FAILED: Some Z3 tests did not meet expectations.")
    sys.exit(1)
