#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <algorithm>

int f(int n);

static double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char** argv) {
    int n      = argc > 1 ? std::atoi(argv[1]) : 100;
    int rounds = argc > 2 ? std::atoi(argv[2]) : 20000;
    int rep    = argc > 3 ? std::atoi(argv[3]) : 100;

    double best = 1e30;
    double total = 0.0;
    long long sink = 0;

    // 预热一次，让缓存/分支预测进入稳定状态
    sink += f(n);

    for (int r = 0; r < rounds; r++) {
        double t0 = now_sec();
        for (int k = 0; k < rep; k++) {
            sink += f(n);
        }
        double t1 = now_sec();
        double dt = (t1 - t0) / rep;   // 单次 f 的平均耗时
        total += dt;
        if (dt < best) best = dt;
    }

    std::printf("n=%d rounds=%d rep=%d best=%.2f ns avg=%.2f ns sink=%lld\n",
                n, rounds, rep, best * 1e9, (total / rounds) * 1e9, sink);
    return 0;
}
