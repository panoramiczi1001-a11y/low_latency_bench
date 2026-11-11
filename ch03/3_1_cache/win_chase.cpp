#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

static void build_linear(char* buf, size_t bytes, size_t stride) {
    const size_t slots = bytes / stride;
    for (size_t i = 0; i + 1 < slots; ++i) {
        *(uint32_t*)(buf + i * stride) = static_cast<uint32_t>((i + 1) * stride);
    }
}

static void build_random(char* buf, size_t bytes, size_t stride) {
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
        std::swap(order[i - 1], order[s % i]);
    }

    for (size_t i = 0; i < slots; ++i) {
        *(uint32_t*)(buf + order[i] * stride) =
            static_cast<uint32_t>(order[(i + 1) % slots] * stride);
    }
}

static double chase(char* buf, size_t bytes, size_t stride, int rounds) {
    const size_t slots = bytes / stride;

    volatile uint32_t pos = 0;

    for (size_t i = 0; i < slots; ++i) {
        pos = *(uint32_t*)(buf + pos);
    }

    uint64_t best = UINT64_MAX;
    for (int r = 0; r < rounds; ++r) {
        const uint64_t t0 = now_ns();
        for (size_t i = 0; i < slots; ++i) {
            pos = *(uint32_t*)(buf + pos);
        }
        const uint64_t t1 = now_ns();
        best = std::min(best, t1 - t0);
    }

    return static_cast<double>(best) / static_cast<double>(slots);
}

int main() {
    const size_t stride = 64;
    const size_t max_bytes = 256ull << 20;

    std::vector<char> buf(max_bytes);

    printf("%8s   %10s   %10s\n", "size", "linear", "random");
    printf("%8s   %10s   %10s\n", "", "ns/hop", "ns/hop");

    for (size_t bytes = 8ull << 20; bytes <= max_bytes; bytes <<= 1) {
        build_linear(buf.data(), bytes, stride);
        const double lin = chase(buf.data(), bytes, stride, 20);

        build_random(buf.data(), bytes, stride);
        const double rnd = chase(buf.data(), bytes, stride, 20);

        printf("%6zu MiB   %10.2f   %10.2f\n", bytes >> 20, lin, rnd);
    }

    return 0;
}
