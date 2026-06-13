ascii-monitor

Terminal ASCII monitor that displays:
- Memory usage and memory maximum
- CPU usage per CPU

Displays usage as horizontal bars with the left-hand side showing percentage. Designed for use in the terminal with ASCII graphics (ncurses).

Language: C (system sampling) and C++ (UI/rendering).

Platforms: Linux, FreeBSD and Solaris. The UI is portable; only the
system-sampling layer (src/monitor.c) is OS-specific:
- Linux   — parses /proc/meminfo and /proc/stat
- FreeBSD — sysctl (hw.physmem, vm.stats.vm.*, kern.cp_time[s])
- Solaris — sysconf (physical pages) for memory, kstat (cpu_stat) for CPU

Build & install dependencies:
- Debian/Ubuntu:  sudo apt-get install -y build-essential g++ libncurses-dev
- Fedora:         sudo dnf install -y gcc gcc-c++ make ncurses-devel
- FreeBSD:        pkg install -y gcc gmake   (ncurses is in the base system)
- Solaris:        pkg install --accept developer/gcc developer/build/gnu-make
                  (ncurses + kstat ship with the OS)

Build:
  make            # on Linux
  gmake           # on FreeBSD and Solaris (the Makefile uses GNU make syntax;
                  # on Solaris it auto-adds -lkstat and the ncurses include path)

Run:
  ./ascii-monitor

Notes:
- ncurses is the only external dependency; everything else (sysctl/kstat/sysconf,
  pthread) is in the base system on each platform.
- On FreeBSD and Solaris "available memory" is approximated from free/inactive
  pages, so the memory bar is an estimate rather than an exact MemAvailable.
