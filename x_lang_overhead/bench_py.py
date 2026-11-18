#!/usr/bin/env python3
"""bench_py.py —— bench_cpp 的 CPython 版本。

同一棵树、同一套下标寻址、同一个递归函数、同一串 LCG 数据，逐行对应。
差别只剩运行时：CPython 每次函数调用要建帧、查类型、维护引用计数。

运行：taskset -c 0 python3 bench_py.py [nodes] [repeats]
"""

import sys
import time


def height_balanced(a, n, i):
    """返回：平衡时是树高，不平衡时是 -1。和 C++ 版逐行对应。"""
    if i >= n:
        return 0
    swap = a[i] & 1
    l = height_balanced(a, n, 2 * i + 1 + swap)
    if l < 0:
        return -1
    r = height_balanced(a, n, 2 * i + 2 - swap)
    if r < 0:
        return -1
    if abs(l - r) > 1:
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
    repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 30

    # 与 C++ 版同一串 LCG（固定种子），保证访问的是同一棵树
    a = []
    x = 12345
    for _ in range(nodes):
        x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
        a.append(x >> 31)

    print("pinned to cpu0:", pin.pin_cpu0())
    print(f"bench_py: nodes={nodes} repeats={repeats} python={sys.version.split()[0]}")

    for _ in range(2):  # 预热
        height_balanced(a, nodes, 0)

    raw = []
    sink = 0
    for _ in range(repeats):
        a[0] ^= 1  # 与 C++ 版一致：破坏循环不变量
        t0 = time.perf_counter_ns()
        h = height_balanced(a, nodes, 0)
        t1 = time.perf_counter_ns()
        sink += h
        raw.append(t1 - t0)

    s = sorted(raw)
    print(f"{'py recursive':<18} n={len(s):7d}  "
          f"p50={pct(s,0.50)/1e6:7.2f}ms  p90={pct(s,0.90)/1e6:8.2f}ms  "
          f"p99={pct(s,0.99)/1e6:8.2f}ms  min={s[0]/1e6:7.2f}ms  "
          f"max={s[-1]/1e6:8.2f}ms")
    print(f"nodes/run={nodes}  =>  p50 per node ~ {pct(s, 0.50) / nodes:.1f} ns  "
          f"(sink={sink})")


if __name__ == "__main__":
    main()
