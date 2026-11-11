// exp1.cpp
// 线性指针追逐：8 MiB 数组，步长 64 字节，量每一跳多久。
// 编译：g++ -O2 -std=c++20 exp1.cpp -o exp1
// 运行：taskset -c 0 ./exp1

#include <chrono>
#include <cstdint>
#include <cstdio>

int main() {
    const size_t bytes = 256ull << 20;    // 256 MiB
    const size_t stride = 64;             // 正好一条 cache line
    const size_t slots = bytes / stride;

    static char buf[256 << 20];

    // 线性链：第 i 个槽指向第 i+1 个槽
    for (size_t i = 0; i + 1 < slots; ++i) {
        *(uint32_t*)(buf + i * stride) = (i + 1) * stride;
    }

    uint32_t pos = 0;

    auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < slots; ++i) {
        pos = *(uint32_t*)(buf + pos);
        asm volatile("" : "+r"(pos));     // 屏障：这次读不能省
    }
    auto t1 = std::chrono::steady_clock::now();

    double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    printf("%.2f ns per hop  (pos=%u)\n", ns / slots, pos);

    return 0;
}
