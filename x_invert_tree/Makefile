CXX ?= g++
CXXFLAGS ?= -O2 -std=c++20 -Wall -Wextra -I../common

.PHONY: all run clean

all: bench_invert

bench_invert: bench_invert.cpp ../common/timer.h
	$(CXX) $(CXXFLAGS) bench_invert.cpp -o bench_invert

run: bench_invert
	taskset -c 0 ./bench_invert 17 100 perfect

clean:
	rm -f bench_invert bench_invert.exe
