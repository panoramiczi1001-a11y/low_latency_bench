# 3.1 处理器缓存与内存层次
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++20 -Wall -Wextra
LINUX_BINS := cache_probe exp1 exp2 exp3 exp4 exp6 exp7

.PHONY: all clean run

all: $(LINUX_BINS)

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

run: all
	@echo "单核跑（避免迁移干扰）：taskset -c 0 ./exp1"

clean:
	rm -f $(LINUX_BINS)
	@rm -f win_chase.exe win_share.exe

# Windows 专用（win_ 前缀，需要 MSVC 或 MinGW）
#   cl /O2 /std:c++20 /EHsc win_share.cpp
