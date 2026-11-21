CXX ?= g++
CXXFLAGS ?= -O2 -std=c++20 -Wall -Wextra -I../common

.PHONY: all run check clean

all: bench_anagram

bench_anagram: bench_anagram.cpp ../common/timer.h
	$(CXX) $(CXXFLAGS) bench_anagram.cpp -o bench_anagram

check: bench_anagram
	./bench_anagram 1 --check

run: bench_anagram
	taskset -c 0 ./bench_anagram 25

clean:
	rm -f bench_anagram bench_anagram.exe
