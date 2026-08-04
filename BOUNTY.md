# The Alu $10,000 "Hack Us" Bounty 🛡️

We claim that the Alu programming language is mathematically proven to be immune to memory-based zero-day exploits (buffer overflows, use-after-free, double-free) via our integration of Z3 Theorem Proving and Automatic Reference Counting (ARC) at the LLVM IR level.

We don't just expect you to trust us. **We want you to prove us wrong.**

## The Challenge
We are offering a **$10,000 USD** bounty to anyone who can successfully trigger a buffer overflow or memory corruption exploit in a binary compiled by the official Alu Compiler.

### The Rules
1. **The Target:** You must write a program in the `alu` language (or use any of our existing `std` library modules or examples, such as `examples/web_server.alu`).
2. **The Execution:** You must compile the program using the native Alu Compiler (`alu_compiler.exe`).
3. **The Exploit:** You must successfully trigger one of the following via malicious input:
   - A Buffer Over-read (e.g., Heartbleed-style memory dump).
   - A Buffer Over-write (e.g., executing arbitrary shellcode by smashing the stack).
   - A Use-After-Free (UAF) or Double-Free crash.
4. **No Inline Assembly:** The exploit must occur in pure Alu code. You cannot use the `asm("...")` escape hatch to intentionally write malicious x86/C++ shellcode to bypass the verifier.

### How to Submit
If you successfully breach the Z3 mathematical verifier and trigger a memory exploit:
1. Create a private GitHub repository containing your `.alu` source code and a Proof of Concept (PoC) script that triggers the exploit.
2. Invite the core Alu team (`@Alu6hel`) to the repository.
3. Open a vulnerability report in our [Security Advisory tab](https://github.com/Alu6hel/alu-language/security/advisories).

### The Reward
*Legal Notice: The $10,000 Bug Bounty will officially open once Alu Security LLC closes its Seed funding round. Until then, valid submissions will receive Hall of Fame status and priority for back-pay.*
If the vulnerability is confirmed valid under the rules above, we will:
1. Pay out the **$10,000 USD** bounty via Wire Transfer or Crypto.
2. Add your name and a link to your profile to our permanent **Hall of Fame**.
3. Publicly disclose the vulnerability and the patch to the cybersecurity community.

Good luck. You're going to need it.
