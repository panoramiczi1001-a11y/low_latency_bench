# cython: language_level=3, boundscheck=False, wraparound=False, initializedcheck=False, cdivision=True
# bench_cython.pyx —— 同一算法的 Cython 版本（全 C 类型）
#
# 关键点：递归函数声明为 cdef + 指针参数 + noexcept nogil，
# 这样每层调用都是普通 C 函数调用，不经过 PyObject 协议。
# 如果写成 def / 用 Python list，Cython 就退化成「给 Python 套了层壳」，
# 提速会从百倍掉到个位数 —— 那正是这份对比想展示的分界线。

from libc.stdint cimport uint32_t
from libc.stdlib cimport malloc, free
import time


cdef int height_balanced(const int* a, int n, int i) noexcept nogil:
    cdef int swap, l, r, d
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


def bench(int nodes, int repeats):
    """返回每次重复的耗时（纳秒）列表。数据与 C++/PyPy 版同一串 LCG。"""
    cdef int* a = <int*>malloc(nodes * sizeof(int))
    if a == NULL:
        raise MemoryError()
    cdef uint32_t x = 12345
    cdef int i, h
    cdef long long t0, t1
    try:
        for i in range(nodes):
            x = x * 1664525 + 1013904223
            a[i] = <int>(x >> 31)

        # 预热
        for i in range(3):
            h = height_balanced(a, nodes, 0)

        raw = []
        for i in range(repeats):
            a[0] = a[0] ^ 1            # 与其它三版一致：破坏循环不变量
            t0 = time.perf_counter_ns()
            h = height_balanced(a, nodes, 0)
            t1 = time.perf_counter_ns()
            raw.append(t1 - t0)
        return raw, h
    finally:
        free(a)
