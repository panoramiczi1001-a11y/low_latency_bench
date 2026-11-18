# setup.py —— 只用来把 .pyx 编译成扩展模块
#
# 用法：
#   pip install cython setuptools      # 一次就好
#   python3 setup.py build_ext --inplace
#   python3 bench_cython_run.py 131071 100

from setuptools import setup
from Cython.Build import cythonize

setup(
    name="bench_cython",
    ext_modules=cythonize(
        "bench_cython.pyx",
        compiler_directives={"language_level": "3"},
        annotate=False,
    ),
)
