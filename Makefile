CC=gcc
CXX=g++
CFLAGS=-Wall -Iinclude
CXXFLAGS=-Wall -Iinclude -std=c++17
LDFLAGS=-lncurses -lpthread

all: ascii-monitor

ascii-monitor: src/monitor.c src/main.cpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/monitor.c -o ascii-monitor $(LDFLAGS)

clean:
	rm -f ascii-monitor
