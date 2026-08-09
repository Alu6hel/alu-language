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

test_files = [
    "cve_buffer_overflow.alu",
    "cve_buffer_underread.alu",
    "cve_loop_overflow.alu"
]

all_passed = True

print("=== Starting Z3 Theorem Prover CVE Test Suite ===")

for test_file in test_files:
    file_path = os.path.join(test_dir, test_file)
    print(f"\nTesting: {test_file}")
    
    # Run the compiler
    cmd = [alu_cxx, "build", file_path]
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    output = result.stdout + result.stderr
    
    # 1. It must fail (non-zero exit code)
    failed = result.returncode != 0
    # 2. It must explicitly mention the Z3 FATAL warning
    caught_by_z3 = "[ALU CXX Z3 FATAL]" in output
    
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

print("\n================================================")
if all_passed:
    print("SUCCESS: All Z3 tests passed! Zero-Day Immunity verified.")
    sys.exit(0)
else:
    print("FAILED: Some Z3 tests did not catch the vulnerabilities.")
    sys.exit(1)
