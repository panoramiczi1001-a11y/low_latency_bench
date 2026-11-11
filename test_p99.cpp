// test_p99.cpp —— 在原来 test.cpp 基础上加三件事：
//   1. 打印时钟实际分辨率（clock_getres），把「100ns 量化」从推测变成证据
//   2. 记录每一轮的耗时，输出 p50 / p90 / p99 / max，而不只是 best
//   3. 量一次计时器本身的开销，扣不扣心里有数
//
// 编译方式与原版一致：
//   g++ -g -std=c++17 -O2 -c test_p99.cpp -o test_x.o
//   g++ -g -std=c++17 -O2 -Dmain=orig_main -c 3vecres.cpp -o impl.o
//   g++ test_x.o impl.o -o test_x
//
// 用法：./test_x [n] [rounds] [rep]

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

int f(int n);

static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// 取分位数：输入必须是已排序的序列
static double percentile(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    size_t idx = static_cast<size_t>(p * (sorted.size() - 1));
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return sorted[idx];
}

// 计时器自身的调用开销：连续取两次时间的差值
static double timer_overhead_ns(int iters) {
    double sum = 0.0;
    for (int i = 0; i < iters; i++) {
        double a = now_sec();
        double b = now_sec();
        sum += (b - a);
    }
    return sum / iters * 1e9;
}

int main(int argc, char** argv) {
    int n      = argc > 1 ? std::atoi(argv[1]) : 100;
    int rounds = argc > 2 ? std::atoi(argv[2]) : 20000;
    int rep    = argc > 3 ? std::atoi(argv[3]) : 100;

    // --- 1. 时钟分辨率：把「100ns 格点」这件事变成可验证的 ---
    struct timespec res;
    clock_gettime(CLOCK_MONOTONIC, &res);
    clock_getres(CLOCK_MONOTONIC, &res);
    double res_ns = res.tv_sec * 1e9 + res.tv_nsec;

    // --- 2. 计时器自身开销 ---
    double timer_ns = timer_overhead_ns(100000);

    std::vector<double> samples;
    samples.reserve(rounds);
    long long sink = 0;

    sink += f(n);   // 预热

    for (int r = 0; r < rounds; r++) {
        double t0 = now_sec();
        for (int k = 0; k < rep; k++) {
            sink += f(n);
        }
        double t1 = now_sec();
        samples.push_back((t1 - t0) / rep * 1e9);   // 单次 f 的平均耗时（纳秒）
    }

    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());

    double sum = 0.0;
    for (double v : samples) sum += v;
    double mean = sum / samples.size();

    // 极差 / 中位数 之比，用来看尾部分布有多散
    double spread = percentile(sorted, 0.99) / (percentile(sorted, 0.5) > 0 ? percentile(sorted, 0.5) : 1);

    std::printf("n=%d rounds=%d rep=%d sink=%lld\n", n, rounds, rep, sink);
    std::printf("  clock_res=%.0f ns  timer_overhead=%.1f ns\n", res_ns, timer_ns);
    std::printf("  best=%7.2f  p50=%7.2f  p90=%7.2f  p99=%8.2f  max=%9.2f  mean=%7.2f  (ns)\n",
                sorted.front(), percentile(sorted, 0.50), percentile(sorted, 0.90),
                percentile(sorted, 0.99), sorted.back(), mean);
    std::printf("  p99/p50=%.2f\n", spread);
    return 0;
}
