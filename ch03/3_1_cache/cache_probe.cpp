// cache_probe.cpp
// 用指针追逐测两件事：
//   1) cache line 有多大（扫步长）
//   2) L1 / L2 / L3 各能装多少（扫工作集）
//
// 编译：g++ -O2 -std=c++20 cache_probe.cpp -o cache_probe
// 运行：taskset -c 0 ./cache_probe

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using Clock = std::chrono::steady_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

// 把 buffer 里的若干个槽串成一条随机的环，槽间距是 stride 字节。
// 每个槽里写的是「下一个槽的字节偏移」。
static void fill_cycle(char* buf, size_t bytes, size_t stride) {
    const size_t slots = bytes / stride;
    std::vector<uint32_t> order(slots);

    for (size_t i = 0; i < slots; ++i) {
        order[i] = static_cast<uint32_t>(i);
    }

    // xorshift，固定种子，每次结果一样
    uint64_t s = 88172645463325252ull;
    for (size_t i = slots; i > 1; --i) {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        size_t j = static_cast<size_t>(s % i);
        std::swap(order[i - 1], order[j]);
    }

    for (size_t i = 0; i < slots; ++i) {
        const size_t from = order[i];
        const size_t to = order[(i + 1) % slots];
        auto* cell = reinterpret_cast<uint32_t*>(buf + from * stride);
        *cell = static_cast<uint32_t>(to * stride);
    }
}

// 沿环走一圈，返回「每一跳平均多少纳秒」。
static double chase_ns_per_hop(char* buf, size_t bytes, size_t stride, int rounds) {
    const size_t slots = bytes / stride;

    // 预热：让页和 cache 都就位
    uint32_t pos = 0;
    for (size_t i = 0; i < slots; ++i) {
        pos = *reinterpret_cast<uint32_t*>(buf + pos);
        asm volatile("" : "+r"(pos));
    }

    // 多轮取最小值，抵消频率波动和调度噪声
    uint64_t best = UINT64_MAX;
    for (int r = 0; r < rounds; ++r) {
        const uint64_t t0 = now_ns();
        for (size_t i = 0; i < slots; ++i) {
            pos = *reinterpret_cast<uint32_t*>(buf + pos);
            asm volatile("" : "+r"(pos));
        }
        const uint64_t t1 = now_ns();
        best = std::min(best, t1 - t0);
    }

    return static_cast<double>(best) / static_cast<double>(slots);
}

int main() {
    const int rounds = 30;

    // 第一组：工作集固定 64 KiB，扫步长。
    // 步长跨过 cache line 之后，每一跳都要换一条线，延迟会跳一下。
    printf("== stride sweep (working set 64 KiB) ==\n");
    const size_t small_bytes = 64 * 1024;
    char* small = static_cast<char*>(std::aligned_alloc(4096, small_bytes));
    std::memset(small, 0, small_bytes);
    for (size_t stride = 4; stride <= 512; stride *= 2) {
        fill_cycle(small, small_bytes, stride);
        const double ns = chase_ns_per_hop(small, small_bytes, stride, rounds);
        printf("stride %4zu B   %6.1f ns/hop\n", stride, ns);
    }
    std::free(small);

    // 第二组：步长固定 64 B，扫工作集。
    // 装不下一层时，延迟会跳一个台阶。
    printf("\n== size sweep (stride 64 B) ==\n");
    for (size_t bytes = 16 * 1024; bytes <= 256ull * 1024 * 1024; bytes *= 2) {
        char* buf = static_cast<char*>(std::aligned_alloc(4096, bytes));
        if (buf == nullptr) {
            printf("alloc failed at %zu\n", bytes);
            break;
        }
        std::memset(buf, 0, bytes);
        fill_cycle(buf, bytes, 64);
        const double ns = chase_ns_per_hop(buf, bytes, 64, rounds);
        printf("%8zu KiB   %6.1f ns/hop\n", bytes / 1024, ns);
        std::free(buf);
    }

    return 0;
}
