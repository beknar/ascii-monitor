ascii-monitor

Specification (for Claude or other assistants):

Goal:
Implement a terminal-based ASCII monitor named "ascii-monitor" that shows memory and per-CPU usage as horizontal bars. The left-hand side of each row must display the percentage numeric value.

Behavior:
- Monitor memory: report current used memory and memory maximum (total). Use /proc/meminfo; compute used = MemTotal - MemAvailable.
- Monitor CPU: for each logical CPU, compute utilization percentage using two snapshots of the cumulative fields in /proc/stat.
- Display: text UI with one line per CPU and one line for memory, showing percentage on the left and a proportional ASCII bar on the right. Update every second by default.
- Terminal-friendly: support resize events and avoid flicker; use ncurses library for drawing.

Implementation notes:
- Language: low-level system sampling in C (src/monitor.c) exposing a small C API; application/renderer in C++ (src/main.cpp) linking against that C API.
- Dependencies: libncurses (dev) and a C/C++ toolchain. No other external runtime libraries.
- Build: simple Makefile using g++ to link C and C++ objects.

Developer instructions:
- Ensure code is robust to missing or malformed /proc entries (fail gracefully).
- Keep parsing code simple and easy to test.
- Provide clear build/run instructions in README.md.
