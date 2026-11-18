// bench_cpp.cpp
// 语言运行时开销：同一棵隐式完全二叉树上的「递归判平衡」，
// C++ 与 CPython 各跑一遍，看调用密集的负载在两种运行时里差多少。
//
// 为什么用隐式完全二叉树（下标寻址）而不是真的建节点+指针：
//   建树本身的开销会混进结果里，而这里要量的是「函数调用 + 分支」本身。
//   下标表示让两版做完全相同的工作，剩下的差别就只有运行时。
//
// 防优化（第一版栽过，见 README「坑」）：
//   1. 子树访问顺序由 a[i] 的最低位决定 —— 让结果依赖数据，编译器不能整段折掉
//   2. 每次重复前翻转 a[0] —— 破坏循环不变量，禁止把调用提到循环外
//   3. noinline + 结果写进 volatile —— 防止内联和死代码消除
//
// 编译：make
// 运行：taskset -c 0 ./bench_cpp [nodes] [repeats]

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "timer.h"

// 返回：平衡时是树高，不平衡时是 -1。
// 两个子节点总是都被访问，只是先后顺序随 a[i] 的最低位交换 ——
// 工作量与「从左到右」完全一致，但编译器无法把结果算成常量。
// 默认**允许**编译器内联。
// 曾试过用 noinline 把它禁掉，想「公平」——但那是过度矫正：
// 编译器消掉小函数调用，本来就是 C++ 快的原因之一，解释器没有这个选项。
// 想对照：加 -DBENCH_NOINLINE。实测差异很小（2.2 vs 2.4 ns），
// 远小于本机噪声——早期一次 6.3 ns 的读数是噪声，不是 noinline 的代价。
#if defined(BENCH_NOINLINE) && defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#elif defined(BENCH_NOINLINE)
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif

NOINLINE
static int height_balanced(const int* a, int n, int i) {
    if (i >= n) return 0;
    const int swap = a[i] & 1;
    const int l = height_balanced(a, n, 2 * i + 1 + swap);
    if (l < 0) return -1;
    const int r = height_balanced(a, n, 2 * i + 2 - swap);
    if (r < 0) return -1;
    const int d = l - r;
    if (d > 1 || d < -1) return -1;
    return (l > r ? l : r) + 1;
}

int main(int argc, char** argv) {
#if defined(_WIN32)
    // 绑到 CPU 0：任务在 P 核/E 核之间迁移会让 p99 抖得很厉害。
    // Linux 侧用 taskset -c 0，效果相同。
    SetProcessAffinityMask(GetCurrentProcess(), 1);
#endif
    const int nodes = (argc > 1) ? std::atoi(argv[1]) : (1 << 17) - 1;  // 131071
    const int repeats = (argc > 2) ? std::atoi(argv[2]) : 200;

    // 固定种子的 LCG：Python 版用同一串数，保证两版访问同一棵树
    std::vector<int> a(static_cast<std::size_t>(nodes));
    std::uint32_t x = 12345u;
    for (auto& v : a) {
        x = x * 1664525u + 1013904223u;
        v = static_cast<int>(x >> 31);
    }

    std::printf("bench_cpp: nodes=%d repeats=%d\n", nodes, repeats);
    std::printf("clock_getres = %.0f ns, timer overhead = %.0f ns\n",
                bench::clock_resolution_ns(), bench::timer_overhead_ns());

    for (int i = 0; i < 3; ++i) {  // 预热：让页表和缓存先热起来
        volatile int sink = height_balanced(a.data(), nodes, 0);
        (void)sink;
    }

    bench::Samples s;
    std::vector<std::uint64_t> raw;
    raw.reserve(static_cast<std::size_t>(repeats));
    volatile int sink = 0;
    for (int r = 0; r < repeats; ++r) {
        a[0] ^= 1;  // 破坏循环不变量：每次的遍历顺序都不同
        const std::uint64_t t0 = bench::now_ns();
        const int h = height_balanced(a.data(), nodes, 0);
        const std::uint64_t t1 = bench::now_ns();
        sink = sink + h;
        s.add(t1 - t0);
        raw.push_back(t1 - t0);
    }

    s.print("cpp recursive");

    std::vector<std::uint64_t> sorted = raw;
    std::sort(sorted.begin(), sorted.end());
    const double med = bench::percentile(sorted, 0.50);
    std::printf("nodes/run=%d  =>  p50 每节点约 %.2f ns\n", nodes,
                static_cast<double>(med) / static_cast<double>(nodes));
    return 0;
}
