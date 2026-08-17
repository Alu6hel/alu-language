import os
import subprocess
import time
import sys

BENCHMARKS = ["fib", "sieve", "matmul"]
COMPILERS = {
    "alu": ["alu_cxx.exe", "build", "-O3"],
    "cpp": ["clang++", "-O3", "-o"],
    "rust": ["rustc", "-O", "-o"]
}
EXTENSIONS = {
    "alu": ".alu",
    "cpp": ".cpp",
    "rust": ".rs"
}

def build(lang, bench_name, src_path, exe_path):
    print(f"Building {bench_name} ({lang})...")
    if lang == "alu":
        cmd = ["..\\alu_cxx.exe", "build", "-O3", src_path]
        try:
            subprocess.run(cmd, check=True)
            generated_exe = os.path.join(os.path.dirname(src_path), f"{bench_name}.exe")
            if os.path.exists(generated_exe):
                for attempt in range(10):
                    try:
                        import shutil
                        shutil.move(generated_exe, exe_path)
                        break
                    except PermissionError:
                        import time
                        time.sleep(0.5)
            return True
        except subprocess.CalledProcessError as e:
            print(f"Failed to build {bench_name} for {lang}:\n{e.stderr.decode()}")
            return False
    elif lang == "cpp":
        cmd = ["clang++", "-O3", src_path, "-o", exe_path]
        try:
            subprocess.run(cmd, check=True)
            return True
        except subprocess.CalledProcessError as e:
            print(f"Failed to build {bench_name} for {lang}:\n{e.stderr.decode()}")
            return False
    elif lang == "rust":
        cmd = ["rustc", "-O", src_path, "-o", exe_path]
        try:
            subprocess.run(cmd, check=True)
            return True
        except subprocess.CalledProcessError as e:
            print(f"Failed to build {bench_name} for {lang}:\n{e.stderr.decode()}")
            return False

def run_bench(exe_path):
    start = time.perf_counter()
    try:
        # Run the process
        proc = subprocess.run([exe_path], check=True)
        end = time.perf_counter()
        return end - start
    except subprocess.CalledProcessError as e:
        print(f"Execution failed for {exe_path}")
        if e.stderr:
            print(e.stderr.decode())
        return None

def main():
    results = {b: {} for b in BENCHMARKS}
    
    for bench in BENCHMARKS:
        print(f"\n--- Running Benchmark: {bench} ---")
        bench_dir = os.path.join(os.getcwd(), bench)
        for lang in ["alu", "cpp", "rust"]:
            src_file = os.path.join(bench_dir, f"{bench}{EXTENSIONS[lang]}")
            exe_file = os.path.join(bench_dir, f"{bench}_{lang}.exe")
            
            if build(lang, bench, src_file, exe_file):
                print(f"Running {lang}...")
                times = []
                for _ in range(3): # Run 3 times, take best
                    t = run_bench(exe_file)
                    if t is not None:
                        times.append(t)
                
                if times:
                    best_time = min(times)
                    results[bench][lang] = best_time
                    print(f"  Best time: {best_time:.4f}s")
                else:
                    results[bench][lang] = float('inf')
            else:
                results[bench][lang] = float('inf')
    
    print("\n\n### LLVM Performance Benchmark Results\n")
    print("| Benchmark | Alu (LLVM) | C++ (Clang) | Rust (rustc) | Alu vs C++ | Alu vs Rust |")
    print("|-----------|------------|-------------|--------------|------------|-------------|")
    
    for bench in BENCHMARKS:
        alu_t = results[bench].get("alu", float('inf'))
        cpp_t = results[bench].get("cpp", float('inf'))
        rs_t = results[bench].get("rust", float('inf'))
        
        alu_str = f"{alu_t:.4f}s" if alu_t != float('inf') else "FAIL"
        cpp_str = f"{cpp_t:.4f}s" if cpp_t != float('inf') else "FAIL"
        rs_str = f"{rs_t:.4f}s" if rs_t != float('inf') else "FAIL"
        
        cmp_cpp = f"{alu_t / cpp_t:.2f}x" if alu_t != float('inf') and cpp_t != float('inf') else "N/A"
        cmp_rs = f"{alu_t / rs_t:.2f}x" if alu_t != float('inf') and rs_t != float('inf') else "N/A"
        
        print(f"| {bench.capitalize():<9} | {alu_str:<10} | {cpp_str:<11} | {rs_str:<12} | {cmp_cpp:<10} | {cmp_rs:<11} |")

if __name__ == "__main__":
    main()
