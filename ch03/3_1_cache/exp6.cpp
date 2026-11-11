// exp6.cpp
// 伪共享：两个线程各加各的计数器，只是布局不同。
//   同一条线：两个计数器挨着，落在同一条 cache line
//   隔开 64B：每个计数器独占一条线
//
// 编译：g++ -O2 -std=c++20 exp6.cpp -o exp6 -pthread
// 运行：taskset -c 0,1 ./exp6

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <pthread.h>
#include <thread>

using Clock = std::chrono::steady_clock;

static constexpr long N = 20000000;

struct Together {
    volatile long a;
    volatile long b;
};

struct Padded {
    alignas(64) volatile long a;
    alignas(64) volatile long b;
};

static void pin(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
}

static void spin(volatile long* p) {
    for (long i = 0; i < N; ++i) {
        *p = *p + 1;
    }
}

template <typename T>
static double measure(T& s) {
    const auto t0 = Clock::now();

    std::thread th0([&] { pin(0); spin(&s.a); });
    std::thread th1([&] { pin(1); spin(&s.b); });

    th0.join();
    th1.join();

    const auto t1 = Clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
    return ns / static_cast<double>(N);
}

int main() {
    Together together{};
    Padded   padded{};

    {
        const uintptr_t pa = reinterpret_cast<uintptr_t>(&together.a);
        const uintptr_t pb = reinterpret_cast<uintptr_t>(&together.b);
        printf("Together: a 在行内偏移 %2zu, b 在行内偏移 %2zu, 同一行=%s\n",
               pa % 64, pb % 64, (pa / 64 == pb / 64) ? "是" : "否");
    }

    for (int r = 0; r < 3; ++r) {
        const double ns_together = measure(together);
        const double ns_padded   = measure(padded);

        printf("同一条线   %7.2f ns/次\n", ns_together);
        printf("隔开 64B   %7.2f ns/次\n", ns_padded);
        printf("倍数       %7.2f\n\n", ns_together / ns_padded);
    }

    printf("checksum %ld %ld %ld %ld\n",
           together.a, together.b, padded.a, padded.b);

    return 0;
}
