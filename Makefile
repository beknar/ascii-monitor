CC=gcc
CXX=g++
CFLAGS=-Wall -Iinclude
CXXFLAGS=-Wall -Iinclude -std=c++17
LDFLAGS=-lncurses -lpthread

# Per-OS adjustments. Uses GNU make syntax, so build with `gmake` on FreeBSD and
# Solaris (on Linux `make` already is GNU make). gmake is installed fleet-wide.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),SunOS)
    # Solaris: ncurses headers live off the default include path, and CPU stats
    # come from kstat (src/monitor.c uses the cpu_stat kstat).
    CXXFLAGS += -I/usr/include/ncurses
    LDFLAGS  += -lkstat
endif

all: ascii-monitor

ascii-monitor: src/monitor.c src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/monitor.c -o ascii-monitor $(LDFLAGS)

clean:
	rm -f ascii-monitor
