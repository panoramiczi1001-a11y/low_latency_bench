#!/usr/bin/env python3
"""bench_cython_run.py —— 跑编译好的 Cython 模块，按和另外三组一样的格式打印。

先编译：python3 setup.py build_ext --inplace
再运行：taskset -c 0 python3 bench_cython_run.py [nodes] [repeats]
"""

import sys

import bench_cython


import pin


def pct(sorted_vals, p):
    if not sorted_vals:
        return 0
    idx = int(p * (len(sorted_vals) - 1))
    return sorted_vals[min(idx, len(sorted_vals) - 1)]


def main():
    nodes = int(sys.argv[1]) if len(sys.argv) > 1 else (1 << 17) - 1
    repeats = int(sys.argv[2]) if len(sys.argv) > 2 else 100

    raw, sink = bench_cython.bench(nodes, repeats)
    s = sorted(raw)
    print("pinned to cpu0:", pin.pin_cpu0())
    print(f"bench_cython: nodes={nodes} repeats={repeats}")
    print(f"{'cython recursive':<18} n={len(s):7d}  "
          f"p50={pct(s, 0.50) / 1e3:8.1f}us  p90={pct(s, 0.90) / 1e3:8.1f}us  "
          f"p99={pct(s, 0.99) / 1e3:8.1f}us  min={s[0] / 1e3:8.1f}us  "
          f"max={s[-1] / 1e3:8.1f}us")
    print(f"nodes/run={nodes}  =>  p50 per node ~ {pct(s, 0.50) / nodes:.2f} ns  "
          f"(sink={sink})")


if __name__ == "__main__":
    main()
