Project: ascii-monitor

Purpose:
- Copilot monitor: memory usage, memory maximum, CPU usage per CPU
- Usage displayed as bars per time period with lefthand as percentage
- To be used in the terminal with ASCII graphics
- Written in C and C++ as appropriate (low-level sampling in C, rendering & app loop in C++)

Design notes for agent collaboration:
- Sampling: read /proc/meminfo for MemTotal and MemAvailable; memory usage = (Total - Available)/Total.
- Per-CPU usage: read /proc/stat for per-cpu cumulative times; compute deltas between samples to get utilization.
- UI: use ncurses for safe terminal drawing and resizing; display one line per CPU and one for memory, left column shows percentage and a horizontal bar to the right.
- Update interval: 1 second default, configurable later.

Dependencies to install:
- build-essential, g++, libncursesw5-dev

Files created:
- include/monitor.h (C header)
- src/monitor.c (C sampling implementation)
- src/main.cpp (C++ ncurses UI and application loop)
- Makefile
- README.md

Development tasks:
- Add command-line flags for sampling interval and history length.
- Add optional color output and warning thresholds.
- Add unit tests for parsing functions (optional).
