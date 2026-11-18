# 本目录：C++ 版本地编译；Cython / PyPy 视环境可选（缺了就跳过）。

CXX ?= g++
CXXFLAGS ?= -O2 -std=c++20 -Wall -Wextra -I../common

.PHONY: all clean run py cython four

all: bench_cpp

bench_cpp: bench_cpp.cpp ../common/timer.h
	$(CXX) $(CXXFLAGS) bench_cpp.cpp -o bench_cpp

# 单组
run: bench_cpp
	taskset -c 0 ./bench_cpp

py:
	taskset -c 0 python3 bench_py.py

cython: bench_cython.pyx setup.py
	python3 setup.py build_ext --inplace

# 四组对比：C++ / Cython / PyPy / CPython（缺哪组脚本会跳过）
four: bench_cpp
	bash four_way.sh

compare: bench_cpp
	@echo "=== C++ ==="
	@taskset -c 0 ./bench_cpp
	@echo
	@echo "=== CPython ==="
	@taskset -c 0 python3 bench_py.py

clean:
	rm -f bench_cpp
	rm -rf build bench_cython*.so bench_cython.c
