# AFL++ Fuzzing Cheat Sheet

## The Dashboard (UI) Explained

When you run `make fuzz-http` or `make fuzz-json`, you see the AFL++ dashboard. Here is what the metrics mean:

### 1. Overall Results (Top Right)
This is your main status panel.
- **saved crashes**: The number of unique bugs found! 🛑 **Goal:** Find > 0.
- **saved hangs**: Inputs that cause the program to freeze/loop (Denial of Service).
- **corpus count**: Number of "interesting" test cases found. AFL generates thousands of inputs, but only keeps ones that find *new code paths*.
  - *Example:* Starting with 3 inputs -> growing to 66 means it found 63 new ways to execute code.

### 2. Process Timing (Top Left)
- **last new find**: How long ago a new interesting path was found. If this says "2 days", the fuzzer might be stuck.

### 3. Stage Progress (Middle Left)
- **now trying**: The current mutation strategy (e.g., `havoc`, `flip`, `splice`).
- **exec speed**: Executions per second.
  - *Target:* > 500/sec is good. < 100/sec is slow.
  - *Current:* ~600/sec (Good!) 🚀

### 4. Item Geometry (Bottom Right)
- **stability**: **Critical Metric**. Should be **100%**.
  - If < 100%, it means the same input produces different behavior (e.g., due to threads, random numbers, or uninitialized memory).

---

## Fuzzing Workflow

### 1. Start Fuzzing
Run these in separate terminals to fuzz continuously:
```bash
make fuzz-http
make fuzz-json
```

### 2. Triage Crashes
If `saved crashes` > 0, check the output directory:
```bash
ls tests/fuzz/out/http/default/crashes/
```
Each file in there is a "killer input" that crashes your server.

### 3. Debug a Crash
Use `valgrind` or `gdb` to see why it crashed:
```bash
# Debug with Valgrind
valgrind ./build/fuzz/fuzz_http_parser < tests/fuzz/out/http/default/crashes/id:000000...
```

### 4. Stop Fuzzing
Press `Ctrl+C` in the terminal window. Fuzzing can be resumed later; AFL saves its state.
