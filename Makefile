CC=gcc
CXX=g++
CFLAGS=-Wall -Iinclude
CXXFLAGS=-Wall -Iinclude -std=c++17
LDFLAGS=-lncursesw -lpthread

# Per-OS adjustments. Uses GNU make syntax, so build with `gmake` on FreeBSD and
# Solaris (on Linux `make` already is GNU make). gmake is installed fleet-wide.
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),SunOS)
    # Solaris: ncurses headers live off the default include path, and CPU stats
    # come from kstat (src/monitor.c uses the cpu_stat kstat).
    CXXFLAGS += -I/usr/include/ncurses
    LDFLAGS  += -lkstat
endif
ifeq ($(UNAME_S),OpenBSD)
    # OpenBSD has no gcc/g++ in base (the gcc pkg is egcc/eg++); use the base
    # clang as cc/c++. ncurses + pthread are in base, so no extra link flags.
    CC=cc
    CXX=c++
endif

all: ascii-monitor

ascii-monitor: src/monitor.c src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/monitor.c -o ascii-monitor $(LDFLAGS)

# --- tests -------------------------------------------------------------------
# The test programs link src/monitor.c, so they need the same per-OS link flags
# as the main binary (notably -lkstat on Solaris for the kstat CPU sampling);
# $(LDFLAGS) carries those. The unused libs (ncurses/pthread) are harmless.
tests/test_unit: tests/test_unit.c src/monitor.c include/monitor.h
	$(CC) $(CFLAGS) tests/test_unit.c src/monitor.c -o tests/test_unit $(LDFLAGS)

tests/test_regression: tests/test_regression.c src/monitor.c include/monitor.h
	$(CC) $(CFLAGS) tests/test_regression.c src/monitor.c -o tests/test_regression $(LDFLAGS)

test-unit: tests/test_unit
	./tests/test_unit

test-regression: tests/test_regression
	./tests/test_regression

test-integration: ascii-monitor
	bash tests/integration_test.sh

test: test-unit test-regression test-integration
	@echo "All test suites passed."

clean:
	rm -f ascii-monitor tests/test_unit tests/test_regression

.PHONY: all test test-unit test-regression test-integration clean
