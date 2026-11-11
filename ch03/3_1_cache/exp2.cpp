// exp2.cpp
// 和 exp1 一样，只是把链打乱，专门用来打败硬件预取器。
// 编译：g++ -O2 -std=c++20 exp2.cpp -o exp2
// 运行：taskset -c 0 ./exp2

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

int main() {
    const size_t bytes = 256ull << 20;    // 256 MiB
    const size_t stride = 64;             // 正好一条 cache line
    const size_t slots = bytes / stride;

    static char buf[256 << 20];

    // 洗牌：决定访问顺序
    std::vector<uint32_t> order(slots);
    for (size_t i = 0; i < slots; ++i) {
        order[i] = static_cast<uint32_t>(i);
    }

    uint64_t s = 88172645463325252ull;
    for (size_t i = slots; i > 1; --i) {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        size_t j = static_cast<size_t>(s % i);
        std::swap(order[i - 1], order[j]);
    }

    // 按洗好的顺序串成一条环
    for (size_t i = 0; i < slots; ++i) {
        const uint32_t from = order[i];
        const uint32_t to   = order[(i + 1) % slots];
        *(uint32_t*)(buf + from * stride) = to * stride;
    }

    uint32_t pos = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < slots; ++i) {
        pos = *(uint32_t*)(buf + pos);
        asm volatile("" : "+r"(pos));
    }
    auto t1 = std::chrono::steady_clock::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    printf("%.2f ns per hop  (pos=%u)\n", ns / slots, pos);

    return 0;
}
