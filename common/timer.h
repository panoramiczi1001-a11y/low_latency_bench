// common/timer.h —— 所有实验共用的计时与统计
//
// 存在意义：让不同实验的数字可比。如果每个实验各写一套计时，两年后
// 那些数据就没法放在一张表里看了。
//
// 三条规矩：
//   1. 单次计时必须同时报告「计时器自身开销」——它决定结果的可信上限
//   2. 分位数（p50/p99/p999）优先于平均值——尾部才是低延迟关心的
//   3. 要报告 clock_getres，因为「分辨率」和「读一次要多久」是两件事
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <vector>

namespace bench {

using Clock = std::chrono::steady_clock;

inline std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            Clock::now().time_since_epoch())
            .count());
}

// 时钟声明分辨率（纳秒）。注意它不代表读一次的成本。
inline double clock_resolution_ns() {
    timespec ts{};
    clock_getres(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) * 1e9 + static_cast<double>(ts.tv_nsec);
}

// 计时器自身开销：连续取两次时间的中位差
inline double timer_overhead_ns(int iters = 20000) {
    std::vector<std::uint64_t> d;
    d.reserve(static_cast<std::size_t>(iters));
    for (int i = 0; i < iters; ++i) {
        const std::uint64_t a = now_ns();
        const std::uint64_t b = now_ns();
        d.push_back(b - a);
    }
    std::sort(d.begin(), d.end());
    return static_cast<double>(d[d.size() / 2]);
}

inline double percentile(const std::vector<std::uint64_t>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    std::size_t idx = static_cast<std::size_t>(p * (sorted.size() - 1));
    if (idx >= sorted.size()) idx = sorted.size() - 1;
    return static_cast<double>(sorted[idx]);
}

// 一批样本：加进去，最后打印成一行
class Samples {
public:
    void add(std::uint64_t ns) { v_.push_back(ns); }
    bool empty() const { return v_.empty(); }
    std::size_t size() const { return v_.size(); }

    void print(const char* label) const {
        if (v_.empty()) {
            std::printf("%-24s (no samples)\n", label);
            return;
        }
        std::vector<std::uint64_t> s = v_;
        std::sort(s.begin(), s.end());
        double sum = 0;
        for (auto x : s) sum += static_cast<double>(x);
        std::printf("%-24s n=%7zu  p50=%7.0f  p90=%8.0f  p99=%9.0f  p999=%10.0f  max=%11.0f  mean=%7.1f  (ns)\n",
                    label, s.size(),
                    percentile(s, 0.50), percentile(s, 0.90),
                    percentile(s, 0.99), percentile(s, 0.999),
                    static_cast<double>(s.back()), sum / static_cast<double>(s.size()));
    }

    void dump(const char* path, const char* label) const {
        if (!path) return;
        FILE* f = std::fopen(path, "a");
        if (!f) return;
        for (auto x : v_) {
            std::fprintf(f, "%s,%llu\n", label, static_cast<unsigned long long>(x));
        }
        std::fclose(f);
    }

private:
    std::vector<std::uint64_t> v_;
};

}  // namespace bench
