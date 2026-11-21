import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import seaborn as sns
import pandas as pd
import numpy as np

sns.set_theme(style="whitegrid", context="talk")
plt.rcParams["font.sans-serif"] = ["Noto Sans CJK JP", "Noto Sans CJK SC",
                                   "Droid Sans Fallback", "DejaVu Sans"]
plt.rcParams["axes.unicode_minus"] = False

OUT = "/sessions/admiring-ecstatic-bell/mnt/量化开发面试/low_latency_bench/x_group_anagram"

# ---- 实测数据：m=1e4，taskset -c 0，40 轮 p50，ns/串 ----
N = [10, 50, 100, 300]
impl = {
    "I 官方标答（排序 key）": [183, 1127, 2513, 7410],
    "A 第一版（排序 key + 拷贝）": [208, 1164, 2491, 7357],
    "F 第二版（计数 key + map）": [347, 371, 372, 471],
    "H 第四版（计数 key + 自写 hash）": [121, 158, 180, 303],
    "D 参考实现（+ reserve / const&）": [97, 122, 138, 226],
}
keyonly = {
    "只算 key：排序": [81.9, 866.0, 2091.9, 6798.6],
    "只算 key：计数": [12.4, 22.4, 36.8, 100.4],
}
COPY = [2.5, 7.6, 8.0, 10.8]

# ============ 图 1：规模曲线 ============
fig, ax = plt.subplots(figsize=(11, 6.6))
colors = sns.color_palette("crest", len(impl))

for (label, ys), c in zip(impl.items(), colors):
    ax.plot(N, ys, "o-", label=label, color=c, lw=2.4, ms=7)

for (label, ys), c in zip(keyonly.items(), ["#4a7fb5", "#47a878"]):
    ax.plot(N, ys, ":", color=c, lw=2.6, label=label, alpha=0.8)

ax.set_xscale("log")
ax.set_yscale("log")
ax.set_xticks(N)
ax.set_xticklabels([str(n) for n in N])
ax.set_xlabel("字符串长度 n")
ax.set_ylabel("每次分组耗时 / 串（ns，对数轴）")
ax.set_title("Group Anagrams：分组 key 的规模曲线（m = 1e4，绑核，40 轮 p50）", pad=14)

# 标注交叉点
ax.annotate("n=10：排序几乎免费，\n第二版反而比第一版慢 1.7×",
            xy=(10, 347), xytext=(12.5, 505),
            fontsize=11, color="#b03030",
            arrowprops=dict(arrowstyle="->", color="#b03030", lw=1.6))
ax.annotate("n=100：官方标答 2513 vs 参考 138\n差 18×；其中 2084 ns 是排序本身",
            xy=(100, 2513), xytext=(30, 4200),
            fontsize=11, color="#1f4e79",
            arrowprops=dict(arrowstyle="->", color="#1f4e79", lw=1.6))

ax.legend(fontsize=9.5, loc="upper left", ncol=1, framealpha=0.92)
ax.grid(True, which="both", alpha=0.25)
fig.tight_layout()
fig.savefig(f"{OUT}/fig_scaling.png", dpi=160)
plt.close(fig)

# ============ 图 2：n=100 的成本拆解 ============
fig, axes = plt.subplots(1, 2, figsize=(13, 5.6),
                         gridspec_kw={"width_ratios": [1.05, 1]})

# 左：官方解 vs 计数解的堆叠拆解
axL = axes[0]
i100 = 2
official_total = impl["I 官方标答（排序 key）"][i100]
sort_only = keyonly["只算 key：排序"][i100]
copy_only = COPY[i100]
official_rest = official_total - sort_only - copy_only

ref_total = impl["D 参考实现（+ reserve / const&）"][i100]
count_only = keyonly["只算 key：计数"][i100]
ref_rest = ref_total - count_only

bars = ["官方标答\n（排序 key）", "参考实现\n（计数 key）"]
sort_part = [sort_only, 0]
copy_part = [copy_only, 0]
key_part = [0, count_only]
rest_part = [official_rest, ref_rest]

b1 = axL.bar(bars, sort_part, color="#3b6ea5", label="排序本身")
b2 = axL.bar(bars, copy_part, bottom=sort_part, color="#8ab4d8", label="排序前的拷贝")
b3 = axL.bar(bars, key_part, color="#3f9e6a", label="计数（扫一遍）")
b4 = axL.bar(bars, rest_part,
             bottom=[sort_part[i] + copy_part[i] + key_part[i] for i in range(2)],
             color="#c9c9c9", label="容器 + 分组 + 收尾")

for i, total in enumerate([official_total, ref_total]):
    axL.text(i, total + 60, f"{total:.0f} ns", ha="center", fontsize=13, weight="bold")
axL.text(0, sort_only / 2, f"{sort_only:.0f}\n(83%)", ha="center", va="center",
         color="white", fontsize=12, weight="bold")
axL.text(1, count_only + ref_rest / 2, f"{ref_rest:.0f}", ha="center", va="center",
         color="#444", fontsize=11, weight="bold")

axL.set_ylabel("ns / 串")
axL.set_title("n=100 时，时间花在哪（m = 1e4）", pad=10)
axL.legend(fontsize=9.5, loc="upper right")
axL.grid(True, axis="y", alpha=0.25)

# 右：只算 key 的对比
axR = axes[1]
x = np.arange(len(N))
w = 0.36
axR.bar(x - w / 2, keyonly["只算 key：排序"], w, label="排序 key", color="#3b6ea5")
axR.bar(x + w / 2, keyonly["只算 key：计数"], w, label="计数 key", color="#3f9e6a")
axR.set_xticks(x)
axR.set_xticklabels([f"n={n}" for n in N])
axR.set_yscale("log")
axR.set_ylabel("ns / 串（对数轴）")
axR.set_title("只算 key、不建表：排序 vs 计数", pad=10)
for i, (a, b) in enumerate(zip(keyonly["只算 key：排序"], keyonly["只算 key：计数"])):
    axR.text(i - w / 2, a * 1.15, f"{a:.0f}", ha="center", fontsize=9.5)
    axR.text(i + w / 2, b * 1.15, f"{b:.0f}", ha="center", fontsize=9.5)
axR.legend(fontsize=10)
axR.grid(True, axis="y", alpha=0.25)

fig.suptitle("官方解为什么慢：83% 的时间在排序，不在容器", y=1.02, fontsize=17, weight="bold")
fig.tight_layout()
fig.savefig(f"{OUT}/fig_breakdown.png", dpi=160, bbox_inches="tight")
plt.close(fig)

# ============ 图 3：优化路径 ============
steps = ["v1\n排序 key", "v2\n计数+map", "计数+array", "四解\n自写 hash", "参考\n顶配"]
path10 = [208, 347, 259, 121, 97]
path100 = [2491, 372, 255, 180, 138]

fig, ax = plt.subplots(figsize=(11.5, 6.4))
x = np.arange(len(steps))
ax.plot(x, path100, "o-", color="#3b6ea5", lw=2.8, ms=10, label="n = 100（约束边界）")
ax.plot(x, path10, "s--", color="#e08a2e", lw=2.4, ms=9, label="n = 10（短串）")

for xi, y in zip(x, path100):
    ax.annotate(f"{y:.0f}", (xi, y), textcoords="offset points", xytext=(0, 13),
                ha="center", fontsize=11, weight="bold", color="#2a4f75")
for xi, y in zip(x, path10):
    ax.annotate(f"{y:.0f}", (xi, y), textcoords="offset points", xytext=(0, -22),
                ha="center", fontsize=11, weight="bold", color="#a8641c")

ax.set_xticks(x)
ax.set_xticklabels(steps, fontsize=11)
ax.set_yscale("log")
ax.set_ylabel("ns / 串（对数轴）")
ax.set_title("优化路径：n=10 上第一步是倒退，n=100 上一步降 6.7×", pad=30)
ax.legend(fontsize=11, loc="upper right")
ax.grid(True, which="both", alpha=0.25)

ax.annotate("这一步是倒退：短串上排序几乎免费，\n计数 key 的常数先付了",
            xy=(1.05, 360), xytext=(1.05, 1150), fontsize=10.5, color="#b03030",
            ha="left",
            arrowprops=dict(arrowstyle="->", color="#b03030", lw=1.6,
                            connectionstyle="arc3,rad=0.15"))
ax.set_ylim(70, 4000)
fig.text(0.5, 0.905,
         "① v1→v2 换算法：−85%（6.7×）   ② 换缓冲区：−31%   ③ map→unordered：−29%   ④ 去拷贝 + reserve：−23%",
         ha="center", fontsize=11.5, color="#444")

fig.tight_layout()
fig.savefig(f"{OUT}/fig_path.png", dpi=160)
plt.close(fig)

print("ok")
