// exp3.cpp
// 线性指针追逐，扫工作集大小：看 8 MiB 到 256 MiB 之间是平滑上升还是跳变。
// 每个尺寸跑多轮，同时打出最小值、最大值，顺便回答「是不是浮动」。
//
// 编译：g++ -O2 -std=c++20 exp3.cpp -o exp3
// 运行：taskset -c 0 ./exp3

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>

static char buf[256 << 20];

static void measure(size_t bytes, size_t stride, int rounds) {
    const size_t slots = bytes / stride;

    // 建线性链：第 i 个槽指向第 i+1 个槽
    for (size_t i = 0; i + 1 < slots; ++i) {
        *(uint32_t*)(buf + i * stride) = static_cast<uint32_t>((i + 1) * stride);
    }

    uint32_t pos = 0;

    // 预热一遍
    for (size_t i = 0; i < slots; ++i) {
        pos = *(uint32_t*)(buf + pos);
        asm volatile("" : "+r"(pos));
    }

    uint64_t best = UINT64_MAX;
    uint64_t worst = 0;

    for (int r = 0; r < rounds; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t i = 0; i < slots; ++i) {
            pos = *(uint32_t*)(buf + pos);
            asm volatile("" : "+r"(pos));
        }
        const auto t1 = std::chrono::steady_clock::now();

        const uint64_t dt =
            std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
        best  = std::min(best, dt);
        worst = std::max(worst, dt);
    }

    const double ns_min = static_cast<double>(best)  / static_cast<double>(slots);
    const double ns_max = static_cast<double>(worst) / static_cast<double>(slots);
    const double gbs    = static_cast<double>(stride) / ns_min;   // 字节/纳秒 = GB/s

    printf("%7zu MiB   min %7.2f ns   max %7.2f ns   %6.1f GB/s   (pos=%u)\n",
           bytes >> 20, ns_min, ns_max, gbs, pos);
}

int main() {
    const size_t stride = 64;
    const int rounds = 20;

    for (size_t bytes = 512ull << 10; bytes <= (256ull << 20); bytes <<= 1) {
        measure(bytes, stride, rounds);
    }

    return 0;
}
