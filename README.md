ascii-monitor

Terminal ASCII monitor for Linux that displays:
- Memory usage and memory maximum
- CPU usage per CPU

Displays usage as horizontal bars with the left-hand side showing percentage. Designed for use in the terminal with ASCII graphics (ncurses).

Language: C (system sampling) and C++ (UI/rendering).

Build & install dependencies (Debian/Ubuntu):
  sudo apt-get update
  sudo apt-get install -y build-essential g++ libncursesw5-dev

Build:
  make

Run:
  ./ascii-monitor

Notes:
- Parses /proc/meminfo and /proc/stat; Linux-only.
- Uses ncurses for terminal drawing; no other external runtime deps.
