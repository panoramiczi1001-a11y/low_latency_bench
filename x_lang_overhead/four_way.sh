#!/usr/bin/env bash
# four_way.sh —— 四组对比：纯血 C++ / Cython / PyPy / 纯 CPython
#
# 用法：
#   bash four_way.sh                 # 默认 131071 节点
#   bash four_way.sh 524287 60       # 指定节点数与重复次数
#
# 前置条件：
#   - g++（已有）
#   - Cython + setuptools：pip install cython setuptools
#   - PyPy3：https://www.pypy.org/download.html（或 conda/brew 安装）
# 缺哪个就跳过那一组，脚本不会中断。

set -u
N=${1:-131071}
R=${2:-100}
cd "$(dirname "$0")" || exit 1

echo "### 四组对比：nodes=$N repeats=$R"
echo

# 1) 纯血 C++
echo "=== 1/4 纯血 C++ (-O2) ==="
if [ ! -x ./bench_cpp ]; then
  make >/dev/null 2>&1 || { echo "编译失败，跳过"; }
fi
if [ -x ./bench_cpp ]; then
  taskset -c 0 ./bench_cpp "$N" "$R" | grep -vE "^bench_cpp:"
else
  echo "缺 bench_cpp，跳过"
fi
echo

# 2) Cython（全 C 类型）
echo "=== 2/4 Cython（cdef + 指针） ==="
if python3 -c "import Cython" >/dev/null 2>&1; then
  python3 setup.py build_ext --inplace >/dev/null 2>&1 || echo "(编译失败)"
  taskset -c 0 python3 bench_cython_run.py "$N" "$R" | grep -vE "^bench_cython:"
else
  echo "缺 Cython，跳过：pip install cython"
fi
echo

# 3) PyPy（JIT）
echo "=== 3/4 PyPy3（JIT） ==="
if command -v pypy3 >/dev/null 2>&1; then
  taskset -c 0 pypy3 bench_py.py "$N" "$R" | grep -vE "^bench_py:"
else
  echo "缺 pypy3，跳过"
fi
echo

# 4) 纯 CPython（解释器）
echo "=== 4/4 纯 CPython（解释器） ==="
taskset -c 0 python3 bench_py.py "$N" "$R" | grep -vE "^bench_py:"
