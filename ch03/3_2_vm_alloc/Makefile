# 3.2 Linux 运行时分配与虚拟内存
CXX      ?= g++
CXXFLAGS ?= -O2 -std=c++20 -Wall -Wextra

.PHONY: all clean

all: exp5

%: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f exp5
