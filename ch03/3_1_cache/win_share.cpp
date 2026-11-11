// win_share.cpp
// False sharing on Windows, with threads pinned to real physical cores.
//
//   usage: win_share.exe <cpuA> <cpuB>
//   e.g.   win_share.exe 0 1     P vs P
//          win_share.exe 4 8     E vs E (different 4-core clusters)
//          win_share.exe 0 12    P vs LP-E
//
// build: cl /O2 /std:c++20 /EHsc win_share.cpp
//        (ASCII only, so no /utf-8 needed)

#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

using Clock = std::chrono::steady_clock;

static const long N_TIGHT  = 20000000;
static const long N_SPARSE = 5000000;
static const int  WORK     = 16;

struct Together {
    volatile long a;
    volatile long b;
};

struct Padded {
    alignas(64) volatile long a;
    alignas(64) volatile long b;
};

static int want_cpu[2] = {0, 1};
static int  got_cpu[2] = {-1, -1};

static void pin(int idx) {
    SetThreadAffinityMask(GetCurrentThread(), (DWORD_PTR)1 << want_cpu[idx]);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    got_cpu[idx] = GetCurrentProcessorNumber();
}

static void spin(volatile long* p, long n, bool sparse) {
    for (long i = 0; i < n; ++i) {
        if (sparse) {
            long x = i;
            for (int k = 0; k < WORK; ++k) {
                x ^= x << 13;      // 非线性，算不出闭式解
                x ^= x >> 7;
                x ^= x << 17;
            }
            if (x == 0x5DEECE66DLL) {   // 几乎永不成立，但强迫编译器算出 x
                *p = *p;
            }
        }
        *p = *p + 1;
    }
}

template <typename T>
static double measure(T& s, long n, bool sparse) {
    got_cpu[0] = -1;
    got_cpu[1] = -1;

    const auto t0 = Clock::now();

    std::thread th0([&] { pin(0); spin(&s.a, n, sparse); });
    std::thread th1([&] { pin(1); spin(&s.b, n, sparse); });

    th0.join();
    th1.join();

    const auto t1 = Clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return ns / static_cast<double>(n);
}

static void run_case(const char* name, long n, bool sparse) {
    Together together{};
    Padded   padded{};

    {
        const uintptr_t pa = reinterpret_cast<uintptr_t>(&together.a);
        const uintptr_t pb = reinterpret_cast<uintptr_t>(&together.b);
        printf("  layout: a@%2zu b@%2zu  same-line=%s\n",
               pa % 64, pb % 64, (pa / 64 == pb / 64) ? "yes" : "NO");
    }

    const double ns_together = measure(together, n, sparse);
    const int cpu0 = got_cpu[0];
    const int cpu1 = got_cpu[1];

    const double ns_padded = measure(padded, n, sparse);

    printf("%-7s  same %7.2f  padded %7.2f  diff %6.2f ns  ratio %5.2f  (ran %d/%d)\n",
           name, ns_together, ns_padded, ns_together - ns_padded,
           ns_together / ns_padded, cpu0, cpu1);
}

int main(int argc, char** argv) {
    if (argc >= 3) {
        want_cpu[0] = std::atoi(argv[1]);
        want_cpu[1] = std::atoi(argv[2]);
    }

    printf("requested cpus: %d and %d\n", want_cpu[0], want_cpu[1]);
    printf("order: -O2, volatile counters, aligned padding = 64B\n\n");

    run_case("tight", N_TIGHT, false);
    run_case("sparse", N_SPARSE, true);

    return 0;
}
