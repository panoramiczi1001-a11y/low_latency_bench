// bench_invert.cpp
// 翻转二叉树（LeetCode 226）的三种写法，在同一棵满二叉树上对比：
//   1) 递归
//   2) 迭代 DFS（显式栈）——注意：栈最大深度是 O(log n)，不是 O(n)
//   3) 迭代 BFS（队列）——队列最大长度是最后一层的宽度，O(n/2)
//
// 想量三件事：时间（per node）、逻辑栈/队列的最大长度、峰值内存。
// 满二叉树上递归深度只有 log2(n)，所以「递归爆栈」不会发生——这正是要写清楚的一点：
// 爆栈取决于树的**形状**，不取决于节点数。
//
// 编译：make
// 运行：./bench_invert [高度h] [重复次数] [perfect|skew]
//       满二叉树节点数 n = 2^(h+1) - 1，深度 = h

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <deque>
#include <algorithm>
#include <cmath>

#if defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "timer.h"

struct Node {
    int l = -1;
    int r = -1;
};

// ---------- 峰值内存（只用来做同一进程内的相对比较）----------
static long peak_rss_kb() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<long>(pmc.PeakWorkingSetSize / 1024);
    return -1;
#else
    rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) == 0) return static_cast<long>(ru.ru_maxrss);
    return -1;
#endif
}

// ---------- 建树 ----------
// perfect：满二叉树，节点 i 的孩子是 2i+1 / 2i+2（隐式下标直接写成显式链）
// skew   ：一条左斜链，深度 = n（用来说明爆栈取决于形状而不是规模）
static void build(std::vector<Node>& t, int h, bool perfect) {
    const int n = (1 << (h + 1)) - 1;
    t.assign(static_cast<std::size_t>(n), Node{});
    if (perfect) {
        for (int i = 0; i < n; ++i) {
            const int l = 2 * i + 1;
            const int r = 2 * i + 2;
            if (l < n) t[i].l = l;
            if (r < n) t[i].r = r;
        }
    } else {
        for (int i = 0; i + 1 < n; ++i) t[i].l = i + 1;
    }
}

// 按 DFS 先序分配节点：模拟 `new Node` 依次建树的真实内存布局。
// 这时 BFS 的访问顺序就不再是顺序访存了 —— 用来验证「BFS 更快」是不是布局假象。
static int build_dfs_order(std::vector<Node>& t, int depth, int& next) {
    const int me = next++;
    if (depth > 0) {
        t[me].l = build_dfs_order(t, depth - 1, next);
        t[me].r = build_dfs_order(t, depth - 1, next);
    }
    return me;
}

// ---------- 三种翻转 ----------
static void inv_rec(std::vector<Node>& t, int i, int d, int& maxd) {
    if (i < 0) return;
    if (d > maxd) maxd = d;
    std::swap(t[i].l, t[i].r);
    inv_rec(t, t[i].l, d + 1, maxd);
    inv_rec(t, t[i].r, d + 1, maxd);
}

// 显式栈做 DFS：最大长度 ≈ 树高，和递归的栈深度同阶，但在堆上
static int inv_dfs(std::vector<Node>& t, int root) {
    std::vector<int> st;
    st.push_back(root);
    int maxsz = 1;
    while (!st.empty()) {
        const int i = st.back();
        st.pop_back();
        std::swap(t[i].l, t[i].r);
        if (t[i].l >= 0) st.push_back(t[i].l);
        if (t[i].r >= 0) st.push_back(t[i].r);
        if (static_cast<int>(st.size()) > maxsz) maxsz = static_cast<int>(st.size());
    }
    return maxsz;
}

// 队列做 BFS：最大长度 ≈ 最后一层的宽度，满二叉树上约 n/2
static int inv_bfs(std::vector<Node>& t, int root) {
    std::deque<int> q;
    q.push_back(root);
    int maxsz = 1;
    while (!q.empty()) {
        const int i = q.front();
        q.pop_front();
        std::swap(t[i].l, t[i].r);
        if (t[i].l >= 0) q.push_back(t[i].l);
        if (t[i].r >= 0) q.push_back(t[i].r);
        if (static_cast<int>(q.size()) > maxsz) maxsz = static_cast<int>(q.size());
    }
    return maxsz;
}

int main(int argc, char** argv) {
#if defined(_WIN32)
    SetProcessAffinityMask(GetCurrentProcess(), 1);
#endif
    const int h = (argc > 1) ? std::atoi(argv[1]) : 17;
    const int repeats = (argc > 2) ? std::atoi(argv[2]) : 100;
    const bool perfect = !(argc > 3 && std::strcmp(argv[3], "skew") == 0);
    const bool dfs_layout = (argc > 4 && std::strcmp(argv[4], "dfs") == 0);

    const std::size_t n = static_cast<std::size_t>((1 << (h + 1)) - 1);
    std::vector<Node> t;

    std::printf("bench_invert: h=%d nodes=%zu repeats=%d shape=%s\n",
                h, n, repeats, perfect ? (dfs_layout ? "perfect/dfs-layout" : "perfect/level-layout") : "skew");
    std::printf("clock_getres = %.0f ns, timer overhead = %.0f ns, peak_rss@start = %ld KB\n",
                bench::clock_resolution_ns(), bench::timer_overhead_ns(), peak_rss_kb());

    bench::Samples s;
    std::vector<std::uint64_t> raw;

    auto make = [&](std::vector<Node>& v) {
        build(v, h, perfect);
        if (perfect && dfs_layout) {
            std::vector<Node> tmp;
            tmp.assign(n, Node{});
            int next = 0;
            build_dfs_order(tmp, h, next);
            v.swap(tmp);
        }
    };

    // ---- 递归 ----
    if (perfect) {  // 链状树上递归会爆栈，这里只在满二叉树上跑
        make(t);
        int maxd = 0;
        inv_rec(t, 0, 0, maxd);  // 预热
        raw.clear();
        s = bench::Samples{};
        for (int r = 0; r < repeats; ++r) {
            maxd = 0;
            const std::uint64_t t0 = bench::now_ns();
            inv_rec(t, 0, 0, maxd);
            const std::uint64_t t1 = bench::now_ns();
            s.add(t1 - t0);
            raw.push_back(t1 - t0);
        }
        s.print("recursive");
        std::vector<std::uint64_t> so = raw;
        std::sort(so.begin(), so.end());
        std::printf("   -> %.2f ns/node, 递归最大深度=%d (log2 n=%.1f)\n",
                    bench::percentile(so, 0.50) / static_cast<double>(n), maxd,
                    std::log2(static_cast<double>(n)));
    }

    // ---- 迭代 DFS ----
    make(t);
    {
        int maxsz = inv_dfs(t, 0);
        raw.clear();
        s = bench::Samples{};
        int mx = 0;
        for (int r = 0; r < repeats; ++r) {
            const std::uint64_t t0 = bench::now_ns();
            mx = inv_dfs(t, 0);
            const std::uint64_t t1 = bench::now_ns();
            s.add(t1 - t0);
            raw.push_back(t1 - t0);
        }
        s.print("iterative DFS");
        std::vector<std::uint64_t> so = raw;
        std::sort(so.begin(), so.end());
        std::printf("   -> %.2f ns/node, 显式栈最大长度=%d (预热时 %d)\n",
                    bench::percentile(so, 0.50) / static_cast<double>(n), mx, maxsz);
    }

    // ---- 迭代 BFS ----
    make(t);
    {
        int maxsz = inv_bfs(t, 0);
        raw.clear();
        s = bench::Samples{};
        int mx = 0;
        for (int r = 0; r < repeats; ++r) {
            const std::uint64_t t0 = bench::now_ns();
            mx = inv_bfs(t, 0);
            const std::uint64_t t1 = bench::now_ns();
            s.add(t1 - t0);
            raw.push_back(t1 - t0);
        }
        s.print("iterative BFS");
        std::vector<std::uint64_t> so = raw;
        std::sort(so.begin(), so.end());
        std::printf("   -> %.2f ns/node, 队列最大长度=%d (预热时 %d)\n",
                    bench::percentile(so, 0.50) / static_cast<double>(n), mx, maxsz);
    }

    std::printf("peak_rss@end = %ld KB  (树本身 %zu B)\n",
                peak_rss_kb(), n * sizeof(Node));
    return 0;
}
