// exp7.cpp
// 同一个伪共享，换不同的核配对跑，看「那一次线传输」到底值多少纳秒。
//
//   用法：./exp7 <cpuA> <cpuB>
//   例：  ./exp7 0 1     P 对 P
//         ./exp7 4 5     E 对 E
//         ./exp7 0 12    P 对低功耗核
//
// 编译：g++ -O2 -std=c++20 exp7.cpp -o exp7 -pthread

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <pthread.h>
#include <sched.h>
#include <thread>

using Clock = std::chrono::steady_clock;

static constexpr long N_TIGHT  = 20000000;
static constexpr long N_SPARSE = 5000000;
static constexpr int  WORK     = 16;     // 依赖链长度，约几十个周期

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
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(want_cpu[idx], &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    got_cpu[idx] = sched_getcpu();
}

static void spin(volatile long* p, long n, bool sparse) {
    for (long i = 0; i < n; ++i) {
        if (sparse) {
            long x = i;
            for (int k = 0; k < WORK; ++k) {
                x = x * 3 + 1;
            }
            asm volatile("" : "+r"(x));
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
        printf("布局检查：a 偏移 %2zu，b 偏移 %2zu，同一行=%s\n",
               pa % 64, pb % 64, (pa / 64 == pb / 64) ? "是" : "否");
    }

    const double ns_together = measure(together, n, sparse);
    const int    cpu0 = got_cpu[0];
    const int    cpu1 = got_cpu[1];

    const double ns_padded = measure(padded, n, sparse);

    printf("%-7s  同线 %7.2f   隔开 %7.2f   差值 %6.2f ns   倍数 %5.2f   (实跑 %d/%d)\n",
           name, ns_together, ns_padded, ns_together - ns_padded,
           ns_together / ns_padded, cpu0, cpu1);
}

int main(int argc, char** argv) {
    if (argc >= 3) {
        want_cpu[0] = std::atoi(argv[1]);
        want_cpu[1] = std::atoi(argv[2]);
    }

    printf("请求的核：%d 和 %d\n", want_cpu[0], want_cpu[1]);
    printf("单位：每次自增的纳秒数\n\n");

    run_case("紧循环", N_TIGHT, false);
    run_case("稀疏",   N_SPARSE, true);

    return 0;
}
