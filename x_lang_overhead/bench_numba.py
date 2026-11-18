#!/usr/bin/env python3
"""bench_numba.py —— 第五组：numba @njit（JIT 编译热点函数）。

和 bench_py.py 完全相同的算法与数据，只是把递归函数交给 LLVM 编译。
关键点：**递归必须给显式签名**，否则 numba 无法在编译期确定返回类型。

运行：python bench_numba.py [nodes] [repeats]
"""

import sys
import time

import numpy as np
from numba import njit

# 显式签名：int32(int32[::1], int32, int32)
# 不给签名的话，自递归函数在 numba 里会因为类型推断失败而退回对象模式（比纯 Python 还慢）。
@njit("int32(int32[::1], int32, int32)", cache=True)
def height_balanced(a, n, i):
    if i >= n:
        return 0
    swap = a[i] & 1
    l = height_balanced(a, n, 2 * i + 1 + swap)
    if l < 0:
        return -1
    r = height_balanced(a, n, 2 * i + 2 - swap)
    if r < 0:
        return -1
    d = l - r
    if d > 1 or d < -1:
        return -1
    return (l if l > r else r) + 1


import pin


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0
    idx = int(p * (len(sorted_vals) - 1))
    return sorted_vals[min(idx, len(sorted_vals) - 1)]


def main():
    nodes = int(sys.argv[1]) if len(sys.argv) > 1 else (1 << 17) - 1
    repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 100

    # 与其它四组同一串 LCG（固定种子 12345）
    a = np.empty(nodes, dtype=np.int32)
    x = 12345
    for k in range(nodes):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        a[k] = x >> 31

    print("pinned to cpu0:", pin.pin_cpu0())
    print(f"bench_numba: nodes={nodes} repeats={repeats}")

    # 第一次调用包含编译，必须排除在计时之外
    t_compile0 = time.perf_counter_ns()
    height_balanced(a, nodes, 0)
    t_compile1 = time.perf_counter_ns()
    print(f"(first call incl. JIT compile: {(t_compile1 - t_compile0) / 1e6:.1f}  ms, excluded)")

    for _ in range(3):  # 预热
        height_balanced(a, nodes, 0)

    raw = []
    sink = 0
    for _ in range(repeats):
        a[0] = a[0] ^ 1  # 与其它四组一致：破坏循环不变量
        t0 = time.perf_counter_ns()
        h = int(height_balanced(a, nodes, 0))
        t1 = time.perf_counter_ns()
        sink += h
        raw.append(t1 - t0)

    s = sorted(raw)
    print(f"{'numba recursive':<18} n={len(s):7d}  "
          f"p50={pct(s, 0.50) / 1e3:8.1f}us  p90={pct(s, 0.90) / 1e3:8.1f}us  "
          f"p99={pct(s, 0.99) / 1e3:8.1f}us  min={s[0] / 1e3:8.1f}us  "
          f"max={s[-1] / 1e3:8.1f}us")
    print(f"nodes/run={nodes}  =>  p50 per node ~ {pct(s, 0.50) / nodes:.2f} ns  "
          f"(sink={sink})")


if __name__ == "__main__":
    main()
