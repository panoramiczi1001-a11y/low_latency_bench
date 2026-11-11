// exp5.cpp
// 随机指针追逐，扫工作集大小，对比 4 KiB 页 与 2 MiB 大页。
// 目的是看那个「8 MiB 到 16 MiB」的台阶会不会被大页推走。
//
// 编译：g++ -O2 -std=c++20 exp5.cpp -o exp5
// 运行：taskset -c 0 ./exp5 4k
//       taskset -c 0 ./exp5 2m

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/mman.h>

using Clock = std::chrono::steady_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

// 数一下这个进程实际拿到了多少大页（kB）。0 表示大页没生效。
static long anon_huge_kb() {
    FILE* f = fopen("/proc/self/smaps", "r");
    if (f == nullptr) {
        return -1;
    }
    char line[512];
    long total = 0;
    while (fgets(line, sizeof line, f) != nullptr) {
        long kb = 0;
        if (sscanf(line, "AnonHugePages: %ld kB", &kb) == 1) {
            total += kb;
        }
    }
    fclose(f);
    return total;
}

static void build_cycle(char* buf, size_t bytes, size_t stride) {
    const size_t slots = bytes / stride;

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

    for (size_t i = 0; i < slots; ++i) {
        const uint32_t from = order[i];
        const uint32_t to   = order[(i + 1) % slots];
        *(uint32_t*)(buf + from * stride) = to * stride;
    }
}

static double measure(char* buf, size_t bytes, size_t stride, int rounds) {
    const size_t slots = bytes / stride;
    uint32_t pos = 0;

    // 预热：也顺便把预取器喂一遍
    for (size_t i = 0; i < slots; ++i) {
        pos = *(uint32_t*)(buf + pos);
        asm volatile("" : "+r"(pos));
    }

    uint64_t best = UINT64_MAX;
    for (int r = 0; r < rounds; ++r) {
        const uint64_t t0 = now_ns();
        for (size_t i = 0; i < slots; ++i) {
            pos = *(uint32_t*)(buf + pos);
            asm volatile("" : "+r"(pos));
        }
        const uint64_t t1 = now_ns();
        best = std::min(best, t1 - t0);
    }

    return static_cast<double>(best) / static_cast<double>(slots);
}

int main(int argc, char** argv) {
    const bool huge = (argc > 1) && (std::strcmp(argv[1], "2m") == 0);

    const size_t want  = 1ull << 30;      // 1 GiB
    const size_t align = 2ull << 20;      // 2 MiB
    const size_t map_len = want + align;

    char* raw = static_cast<char*>(
        mmap(nullptr, map_len, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (raw == MAP_FAILED) {
        std::perror("mmap");
        return 1;
    }

    const uintptr_t p = reinterpret_cast<uintptr_t>(raw);
    const uintptr_t aligned = (p + align - 1) & ~(align - 1);
    char* buf = reinterpret_cast<char*>(aligned);

    if (huge) {
        if (madvise(buf, want, MADV_HUGEPAGE) != 0) {
            std::perror("madvise");
        }
    }

    printf("mode: %s\n", huge ? "2m" : "4k");

    for (size_t bytes = 8ull << 20; bytes <= (256ull << 20); bytes <<= 1) {
        build_cycle(buf, bytes, 64);
        const double ns = measure(buf, bytes, 64, 5);
        printf("%4zu MiB   %7.2f ns/hop\n", bytes >> 20, ns);
    }

    printf("AnonHugePages: %ld kB\n", anon_huge_kb());

    return 0;
}
