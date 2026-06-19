ascii-monitor

Terminal ASCII monitor that displays:
- Memory usage and memory maximum
- CPU usage per CPU
- Disk usage per mounted filesystem (auto-discovered)

Displays usage as horizontal bars with the left-hand side showing percentage. Designed for use in the terminal with ASCII graphics (ncurses).

Language: C (system sampling) and C++ (UI/rendering).

Platforms: Linux, FreeBSD, OpenBSD and Solaris. The UI is portable; only the
system-sampling layer (src/monitor.c) is OS-specific:
- Linux   — /proc/meminfo, /proc/stat, /proc/mounts (+ statvfs)
- FreeBSD — sysctl (hw.physmem, vm.stats.vm.*, kern.cp_time[s]), getmntinfo
- OpenBSD — sysctl (VM_UVMEXP, KERN_CPTIME[2]), getmntinfo
- Solaris — sysconf (physical pages), kstat (cpu_stat), /etc/mnttab (+ statvfs)

Build & install dependencies:
- Debian/Ubuntu:  sudo apt-get install -y build-essential g++ libncursesw-dev
- Fedora:         sudo dnf install -y gcc gcc-c++ make ncurses-devel
- FreeBSD:        pkg install -y gcc gmake   (ncurses is in the base system)
- OpenBSD:        pkg_add gmake              (clang + ncurses are in the base system)
- Solaris:        pkg install --accept developer/gcc developer/build/gnu-make
                  (ncurses + kstat ship with the OS)

Build:
  make            # on Linux
  gmake           # on FreeBSD, OpenBSD and Solaris (the Makefile uses GNU make
                  # syntax; it auto-picks the per-OS compiler/link flags, e.g.
                  # base clang on OpenBSD and -lkstat + the ncurses include path
                  # on Solaris)

Run:
  ./ascii-monitor

Graphics:
- The UI is framed in an ANSI box with a centered title. CPU, memory and disk
  usage are shown as bars of hollow squares (Unicode U+25A1) whose outlines are
  colored by the utilization state — green (good), yellow (warning), red (alert)
  — wrapped in brackets, filling into blank space as utilization rises.
- The hollow square uses the wide-character ncurses API (cchar_t/add_wch,
  linked against -lncursesw) and a UTF-8 locale; on a non-UTF-8 terminal it
  falls back to a solid ACS block. The frame, brackets and track are plain
  terminfo ACS, so they render on Linux, FreeBSD, OpenBSD and Solaris.

Color:
- Bars are colored by utilization: green (low), yellow (moderate), red (high).
- Row labels use a cyan accent; percentages and counts keep the terminal's
  default foreground so they stay readable.
- Colors are drawn over the terminal's *default* background, so the output
  looks right on both dark and light themes.
- Set NO_COLOR=1 (https://no-color.org) to force monochrome output; the program
  also falls back to monochrome on terminals without color support.

Test:
  make test              # unit + regression + integration
  make test-unit         # pure-logic tests (thresholds, bar/disk math)
  make test-regression   # golden values + live CPU/mem/disk sampling
  make test-integration  # drives the real ncurses binary in a pty (Linux only)

Release notes:
- v1.6 — blank bar track: the dim checkerboard behind the bars is removed; empty
  cells are now blank, so the squares fill into clear space inside the brackets.
- v1.5 — hollow-square bars: utilization is drawn as hollow squares (Unicode
  U+25A1) outlined in green/yellow/red for good/warning/alert, via wide-character
  ncurses (-lncursesw); falls back to a solid block on non-UTF-8 terminals.
- v1.4 — disk monitoring + gradient block bars: auto-discovers each host's
  mounted, real (disk-backed) filesystems and shows a used-space bar for each;
  every bar is now a gradient of discrete colored blocks (green -> yellow -> red
  by position).
- v1.3 — OpenBSD support: builds and runs on OpenBSD (sysctl VM_UVMEXP +
  KERN_CPTIME/KERN_CPTIME2); OpenBSD joined the CI build/release fleet.
- v1.2 — ANSI block graphics: bordered frame + a titled UI and ACS block bars,
  replacing the original '#'/space bars.
- v1.1 — color support: utilization-colored bars over the terminal's default
  background, with a NO_COLOR / no-color-terminal monochrome fallback.
- v1.0 — initial release: per-CPU and memory usage bars (ncurses TUI).

Prebuilt per-platform binaries are attached to every release:
https://github.com/beknar/ascii-monitor/releases

Notes:
- ncurses is the only external dependency; everything else (sysctl/kstat/sysconf,
  getmntinfo/statvfs, pthread) is in the base system on each platform.
- On FreeBSD, OpenBSD and Solaris "available memory" is approximated from
  free/inactive pages, so the memory bar is an estimate rather than an exact
  MemAvailable.
