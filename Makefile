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

# --- tests -------------------------------------------------------------------
tests/test_unit: tests/test_unit.c src/monitor.c include/monitor.h
	$(CC) $(CFLAGS) tests/test_unit.c src/monitor.c -o tests/test_unit

tests/test_regression: tests/test_regression.c src/monitor.c include/monitor.h
	$(CC) $(CFLAGS) tests/test_regression.c src/monitor.c -o tests/test_regression

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
