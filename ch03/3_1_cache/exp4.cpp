// exp4.cpp
// 两组实验：
//   A. 二维数组，行优先 vs 列优先（缓存的空间局部性）
//   B. 大块内存，每页只碰一次，第一遍 vs 第二遍（缺页的代价）
//
// 编译：g++ -O2 -std=c++20 exp4.cpp -o exp4
// 运行：taskset -c 0 ./exp4

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>
#include <sys/resource.h>

using Clock = std::chrono::steady_clock;

static uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch()).count());
}

static long minor_faults() {
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_minflt;
}

static void* fresh_map(size_t bytes) {
    void* p = mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        std::perror("mmap");
        std::exit(1);
    }
    return p;
}

int main() {
    // ---------- A. 行优先 vs 列优先 ----------
    const size_t N = 2048;                       // 2048 x 2048 个 int，共 16 MiB
    const size_t matrix_bytes = N * N * sizeof(int);

    {
        int* m = static_cast<int*>(fresh_map(matrix_bytes));

        const long f0 = minor_faults();
        const uint64_t t0 = now_ns();
        long sum = 0;
        for (size_t r = 0; r < N; ++r) {
            for (size_t c = 0; c < N; ++c) {
                sum += m[r * N + c];
            }
        }
        const uint64_t t1 = now_ns();
        const long f1 = minor_faults();

        const double ms = double(t1 - t0) / 1e6;
        printf("行优先   %8.2f ms   %6.2f ns/次   缺页 %ld   sum=%ld\n",
               ms, double(t1 - t0) / double(N * N), f1 - f0, sum);

        munmap(m, matrix_bytes);
    }

    {
        int* m = static_cast<int*>(fresh_map(matrix_bytes));

        const long f0 = minor_faults();
        const uint64_t t0 = now_ns();
        long sum = 0;
        for (size_t c = 0; c < N; ++c) {
            for (size_t r = 0; r < N; ++r) {
                sum += m[r * N + c];
            }
        }
        const uint64_t t1 = now_ns();
        const long f1 = minor_faults();

        const double ms = double(t1 - t0) / 1e6;
        printf("列优先   %8.2f ms   %6.2f ns/次   缺页 %ld   sum=%ld\n",
               ms, double(t1 - t0) / double(N * N), f1 - f0, sum);

        munmap(m, matrix_bytes);
    }

    // ---------- B. 每页碰一次 ----------
    const size_t big_bytes = 1ull << 30;         // 1 GiB
    const size_t page = 4096;

    {
        char* p = static_cast<char*>(fresh_map(big_bytes));

        long f0 = minor_faults();
        const uint64_t t0 = now_ns();
        for (size_t off = 0; off < big_bytes; off += page) {
            p[off] = 1;
        }
        const uint64_t t1 = now_ns();
        long f1 = minor_faults();

        const uint64_t t2_0 = now_ns();
        for (size_t off = 0; off < big_bytes; off += page) {
            p[off] = 2;
        }
        const uint64_t t2_1 = now_ns();
        long f2 = minor_faults();

        // 主动把页扔掉，下一次触碰会重新缺页
        madvise(p, big_bytes, MADV_DONTNEED);

        const uint64_t t3_0 = now_ns();
        for (size_t off = 0; off < big_bytes; off += page) {
            p[off] = 3;
        }
        const uint64_t t3_1 = now_ns();
        long f3 = minor_faults();

        printf("\n每页碰一次 (1 GiB)\n");
        printf("第一遍   %8.2f ms   缺页 %ld\n", double(t1 - t0) / 1e6, f1 - f0);
        printf("第二遍   %8.2f ms   缺页 %ld\n", double(t2_1 - t2_0) / 1e6, f2 - f1);
        printf("第三遍   %8.2f ms   缺页 %ld   (MADV_DONTNEED 之后)\n",
               double(t3_1 - t3_0) / 1e6, f3 - f2);

        munmap(p, big_bytes);
    }

    return 0;
}
