import subprocess
import time

p = subprocess.Popen(
    ["lldb", "tests/test_debug.exe"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

def read_until(prompt, timeout=2):
    out = ""
    start = time.time()
    while True:
        if time.time() - start > timeout:
            break
        char = p.stdout.read(1)
        if not char:
            break
        out += char
        if prompt in out:
            break
    return out

def send_cmd(cmd):
    print(">", cmd)
    p.stdin.write(cmd + "\n")
    p.stdin.flush()
    res = read_until("(lldb)")
    print(res)
    return res

read_until("(lldb)")
send_cmd("b main")
send_cmd("r")
send_cmd("n")
send_cmd("s")
send_cmd("frame var")
send_cmd("bt")
send_cmd("q")
