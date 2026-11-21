// bench_anagram.cpp
// Group Anagrams (LeetCode 49) 的几种写法，在同一份输入上对比。
//
// 为什么值得单开一个实验：OJ 只告诉你「通过了」，不告诉你差在哪。
// 这道题的题面把目标写成 O(m·n)，比排序 key 的 O(m·n·log n) 少一个 log。
// 但 n 的约束只有 100，log2(100) ≈ 6.6——理论上少一半，实测未必。
// 这个实验要量的就是：那个 log 到底值不值，以及它被什么盖住了。
//
// 编译：make
// 运行：./bench_anagram [重复次数] [--check]
//       taskset -c 0 ./bench_anagram 25      （绑核，和 x_lang_overhead 同口径）

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "timer.h"

// ---------- 分配计数（只在单独一遍里跑，不计时）----------
static std::uint64_t g_allocs = 0;
static bool g_count_alloc = false;

void* operator new(std::size_t n) {
    if (g_count_alloc) ++g_allocs;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using Group = std::vector<std::string>;
using Result = std::vector<Group>;
using Input = std::vector<std::string>;

static volatile std::uint64_t g_sink = 0;

// ---------- 数据 ----------
// uniform: 每个字符独立均匀取 26 个小写字母
// perm:    从若干个基串出发随机打乱 —— 同一组里的串全都互不相同，
//          桶会一直 push_back，考验的是 vector 增长和哈希分布
static Input make_uniform(std::size_t m, std::size_t n, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> d(0, 25);
    Input v;
    v.reserve(m);
    for (std::size_t i = 0; i < m; ++i) {
        std::string s(n, 'a');
        for (std::size_t j = 0; j < n; ++j) s[j] = static_cast<char>('a' + d(rng));
        v.push_back(std::move(s));
    }
    return v;
}

static Input make_perm(std::size_t m, std::size_t n, std::size_t bases, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> d(0, 25);
    std::vector<std::string> base;
    for (std::size_t b = 0; b < bases; ++b) {
        std::string s(n, 'a');
        for (std::size_t j = 0; j < n; ++j) s[j] = static_cast<char>('a' + d(rng));
        base.push_back(std::move(s));
    }
    Input v;
    v.reserve(m);
    for (std::size_t i = 0; i < m; ++i) {
        std::string s = base[i % bases];
        std::shuffle(s.begin(), s.end(), rng);
        v.push_back(std::move(s));
    }
    return v;
}

// ---------- 校验用：把解规范化，实现之间的顺序差异不影响比较 ----------
static std::vector<std::string> canonical(const Result& r) {
    std::vector<std::string> out;
    out.reserve(r.size());
    for (const auto& g : r) {
        std::vector<std::string> t = g;
        std::sort(t.begin(), t.end());
        std::string joined;
        for (const auto& s : t) { joined += s; joined += '\x01'; }
        out.push_back(std::move(joined));
    }
    std::sort(out.begin(), out.end());
    return out;
}

// =====================================================================
// A. 排序 key —— 现在仓库里的那版（原样，只加了 const& / move 之前的对照）
// =====================================================================
static Result v_sort_copy(Input& strs) {
    std::unordered_map<std::string, Group> seen;
    Result ans;
    for (auto s : strs) {                 // 拷贝每个 string
        std::string k = s;
        std::sort(k.begin(), k.end());
        seen[k].push_back(s);             // 又拷一次
    }
    for (auto kv : seen) ans.push_back(kv.second);   // 整组再拷一次
    return ans;
}

// B. 排序 key + 去掉全部多余拷贝（只改引用和 move）
static Result v_sort_move(Input& strs) {
    std::unordered_map<std::string, Group> seen;
    seen.reserve(strs.size());
    for (const auto& s : strs) {
        std::string k = s;                // 必须拷贝：输入是引用，不能就地排
        std::sort(k.begin(), k.end());
        seen[k].push_back(s);
    }
    Result ans;
    ans.reserve(seen.size());
    for (auto& kv : seen) ans.push_back(std::move(kv.second));
    return ans;
}

// C. 计数 key + std::map（不需要自己写 hash）
static Result v_count_map(Input& strs) {
    std::map<std::array<int, 26>, Group> seen;
    std::array<int, 26> k{};
    for (const auto& s : strs) {
        k.fill(0);
        for (unsigned char c : s) ++k[c - 'a'];
        seen[k].push_back(s);
    }
    Result ans;
    ans.reserve(seen.size());
    for (auto& kv : seen) ans.push_back(std::move(kv.second));
    return ans;
}

// D. 计数 key + unordered_map（自己写 hash）
struct ArrayHash {
    std::size_t operator()(const std::array<int, 26>& a) const {
        std::size_t h = 0;
        for (int x : a) h = h * 131 + static_cast<std::size_t>(x);
        return h;
    }
};

static Result v_count_hash(Input& strs) {
    std::unordered_map<std::array<int, 26>, Group, ArrayHash> seen;
    seen.reserve(strs.size());
    std::array<int, 26> k{};
    for (const auto& s : strs) {
        k.fill(0);
        for (unsigned char c : s) ++k[c - 'a'];
        seen[k].push_back(s);
    }
    Result ans;
    ans.reserve(seen.size());
    for (auto& kv : seen) ans.push_back(std::move(kv.second));
    return ans;
}

// E. 计数 key 拼成带分隔符的串 + unordered_map<string>
static Result v_count_string(Input& strs) {
    std::unordered_map<std::string, Group> seen;
    seen.reserve(strs.size());
    std::string k;
    std::array<int, 26> cnt{};
    for (const auto& s : strs) {
        cnt.fill(0);
        for (unsigned char c : s) ++cnt[c - 'a'];
        k.clear();
        for (int x : cnt) { k += std::to_string(x); k += ','; }
        seen[k].push_back(s);
    }
    Result ans;
    ans.reserve(seen.size());
    for (auto& kv : seen) ans.push_back(std::move(kv.second));
    return ans;
}

// =====================================================================
// F. 他本人的第二版（submission-1.cpp，原样照抄）
// =====================================================================
static std::vector<int> getHash(std::string s) {
    std::vector<int> ret(26, 0);
    for (char i : s) ret[i - 'a']++;
    return ret;
}

static Result v_user_v2(Input& strs) {
    std::map<std::vector<int>, Group> seen;
    Result ans;
    for (auto i : strs) seen[getHash(i)].push_back(i);
    for (auto i : seen) ans.push_back(i.second);
    return ans;
}

// H. 他的第四版（submission-3.cpp，原样照抄，只把 struct 改名避免和上面的 ArrayHash 撞）
struct ArrayHashUser {
    std::size_t operator()(const std::array<int, 26>& a) const {
        std::size_t h = 0;
        for (int x : a) h = h * 131 + static_cast<std::size_t>(x);
        return h;
    }
};

static std::array<int, 26> getHashUser(std::string s) {
    std::array<int, 26> ret{};
    for (char i : s) ret[i - 'a']++;
    return ret;
}

static Result v_user_v4(Input& strs) {
    std::unordered_map<std::array<int, 26>, Group, ArrayHashUser> seen;
    Result ans;
    for (auto i : strs) seen[getHashUser(i)].push_back(i);
    for (auto& i : seen) ans.push_back(std::move(i.second));
    return ans;
}

// I. NeetCode 官方标答（submission-4.cpp，原样照抄）
static Result v_official(Input& strs) {
    std::unordered_map<std::string, Group> res;
    for (const auto& s : strs) {
        std::string sortedS = s;
        std::sort(sortedS.begin(), sortedS.end());
        res[sortedS].push_back(s);
    }
    Result result;
    for (auto& pair : res) result.push_back(pair.second);
    return result;
}

// G. 只把 vector<int> 换成 array<int,26>，其余不动 —— 用来分离「分配」和「map」两个成本
static Result v_user_v2_arrayonly(Input& strs) {
    std::map<std::array<int, 26>, Group> seen;
    Result ans;
    std::array<int, 26> k{};
    for (const auto& s : strs) {
        k.fill(0);
        for (char c : s) ++k[c - 'a'];
        seen[k].push_back(s);
    }
    for (auto& kv : seen) ans.push_back(std::move(kv.second));
    return ans;
}

// ---------- 跑一遍 ----------
struct Impl {
    const char* name;
    Result (*fn)(Input&);
};

static std::uint64_t checksum(const Result& r) {
    std::uint64_t h = 1469598103934665603ull;
    for (const auto& g : r) {
        h ^= g.size();
        h *= 1099511628211ull;
        for (const auto& s : g) h ^= static_cast<std::uint64_t>(s.size());
    }
    return h;
}

int main(int argc, char** argv) {
    int reps = 20;
    bool check_only = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--check") == 0) check_only = true;
        else reps = std::atoi(argv[i]);
    }

    const Impl impls[] = {
        {"A sort+unordered(copy)", v_sort_copy},
        {"B sort+unordered(move)", v_sort_move},
        {"C count+map",            v_count_map},
        {"D count+unordered",      v_count_hash},
        {"E count+string key",     v_count_string},
        {"F 他的二解(vector+map)", v_user_v2},
        {"G 同二解但换 array",     v_user_v2_arrayonly},
        {"H 他的四解(自写hash)",   v_user_v4},
        {"I 官方标答(排序key)",    v_official},
    };
    const int NI = static_cast<int>(sizeof(impls) / sizeof(impls[0]));

    struct Cfg { std::size_t m, n; const char* tag; };
    const Cfg cfgs[] = {
        {1000,  10,  "m=1e3 n=10  (约束内,短串)"},
        {10000, 10,  "m=1e4 n=10  (约束内,短串)"},
        {10000, 50,  "m=1e4 n=50  (约束内,中串)"},
        {10000, 100, "m=1e4 n=100 (约束边界)"},
        {10000, 300, "m=1e4 n=300 (超约束,长串)"},
        {20000, 100, "m=2e4 n=100 (超 m 两倍)"},
    };

    std::printf("时钟分辨率 = %.0f ns，计时器自身开销 = %.0f ns/次\n",
                bench::clock_resolution_ns(), bench::timer_overhead_ns());
    std::printf("(每格 = 一次完整调用的 p50，单位 ns；括号内是 ns/串)\n\n");

    for (const auto& c : cfgs) {
        Input base = make_uniform(c.m, c.n, 12345);

        // --- 正确性：所有实现必须给出同一个规范化解 ---
        auto ref = canonical(impls[0].fn(base));
        bool ok = true;
        for (int i = 1; i < NI; ++i) {
            Input tmp = base;
            if (canonical(impls[i].fn(tmp)) != ref) {
                std::printf("!! %s 与 %s 结果不一致\n", impls[i].name, impls[0].name);
                ok = false;
            }
        }
        if (check_only) { std::printf("%-30s %s\n", c.tag, ok ? "ok" : "FAIL"); continue; }

        std::printf("---- %s ----\n", c.tag);
        std::printf("%-26s %10s %10s %10s %12s\n",
                    "", "p50(ms)", "p99(ms)", "max(ms)", "p50/串(ns)");
        for (int i = 0; i < NI; ++i) {
            std::vector<std::uint64_t> lat;
            lat.reserve(reps);
            Input work;
            for (int r = 0; r < reps; ++r) {
                work = base;                       // 每轮给一份干净输入，开销不计时
                const std::uint64_t t0 = bench::now_ns();
                Result res = impls[i].fn(work);
                const std::uint64_t t1 = bench::now_ns();
                lat.push_back(t1 - t0);
                g_sink ^= checksum(res);
            }
            std::sort(lat.begin(), lat.end());
            const double p50 = bench::percentile(lat, 0.50);
            const double p99 = bench::percentile(lat, 0.99);
            const double mx  = static_cast<double>(lat.back());
            std::printf("%-26s %10.3f %10.3f %10.3f %12.1f\n",
                        impls[i].name, p50 / 1e6, p99 / 1e6, mx / 1e6,
                        p50 / static_cast<double>(c.m));
        }
        std::printf("\n");
    }

    // --- 只算 key、不建表：把「排序成本」从「容器成本」里拆出来 ---
    // 官方解慢在哪，光看总数说不清。这一节只做 key 的计算，不插入任何容器，
    // 差值就是容器 + 拷贝的代价。
    if (!check_only) {
        std::printf("==== 只算 key，不建表（ns/串，40 轮 p50）====\n");
        std::printf("%-24s %10s %10s %10s %10s\n", "", "n=10", "n=50", "n=100", "n=300");
        const std::size_t ns[4] = {10, 50, 100, 300};
        const char* mode_name[3] = {"排序 key（官方解法）", "计数 key（array）", "只拷贝、不排序"};
        for (int mode = 0; mode < 3; ++mode) {
            std::printf("%-24s", mode_name[mode]);
            for (int ci = 0; ci < 4; ++ci) {
                Input base = make_uniform(10000, ns[ci], 4242);
                std::vector<std::uint64_t> lat;
                for (int r = 0; r < 40; ++r) {
                    std::uint64_t acc = 0;
                    const std::uint64_t t0 = bench::now_ns();
                    if (mode == 0) {
                        for (const auto& s : base) {
                            std::string k = s;
                            std::sort(k.begin(), k.end());
                            acc += static_cast<unsigned char>(k[0]);
                            acc += k.size();
                        }
                    } else if (mode == 1) {
                        std::array<int, 26> c{};
                        for (const auto& s : base) {
                            c.fill(0);
                            for (unsigned char ch : s) ++c[ch - 'a'];
                            acc += static_cast<std::uint64_t>(c[0] + c[25]);
                        }
                    } else {
                        for (const auto& s : base) {
                            std::string k = s;          // 排序前必须先拷贝一份
                            acc += static_cast<unsigned char>(k[0]);
                            acc += k.size();
                        }
                    }
                    const std::uint64_t t1 = bench::now_ns();
                    lat.push_back(t1 - t0);
                    g_sink ^= acc;
                }
                std::sort(lat.begin(), lat.end());
                std::printf(" %10.1f", bench::percentile(lat, 0.50) / 10000.0);
            }
            std::printf("\n");
        }
        std::printf("\n");
    }

    // --- 分配次数（单独一遍，不计时）---
    std::printf("==== 每次调用的 malloc 次数（m=1e4, n=100）====\n");
    {
        Input base = make_uniform(10000, 100, 999);
        for (int i = 0; i < NI; ++i) {
            Input work = base;
            g_allocs = 0;
            g_count_alloc = true;
            Result res = impls[i].fn(work);
            g_count_alloc = false;
            std::printf("%-26s %8llu 次\n", impls[i].name,
                        static_cast<unsigned long long>(g_allocs));
            g_sink ^= checksum(res);
        }
    }

    std::printf("\nsink = %llu（防止整段被优化掉）\n",
                static_cast<unsigned long long>(g_sink));
    return 0;
}
