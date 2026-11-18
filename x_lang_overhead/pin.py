"""pin.py —— 把当前进程绑到 CPU 0。

为什么需要：C++ 版在 main() 里用 SetProcessAffinityMask 绑了核，
Python 进程如果到处飘，p50 和 p99 都会被 E 核迁移和调度抖动污染，
两边就不是在比语言，而是在比调度器。不用装 psutil，ctypes 直接调。
"""

import os
import sys


def pin_cpu0() -> bool:
    """成功返回 True；平台不支持或没权限时返回 False（不抛异常）。"""
    try:
        if sys.platform == "win32":
            import ctypes

            k32 = ctypes.windll.kernel32
            # 必须声明签名：GetCurrentProcess 返回的是 64 位伪句柄 -1，
            # ctypes 默认按 32 位 int 处理会把它截断，SetProcessAffinityMask 直接失败。
            k32.GetCurrentProcess.restype = ctypes.c_void_p
            k32.GetCurrentProcess.argtypes = []
            k32.SetProcessAffinityMask.restype = ctypes.c_int
            k32.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
            return bool(k32.SetProcessAffinityMask(k32.GetCurrentProcess(), 1))
        os.sched_setaffinity(0, {0})
        return True
    except Exception:
        return False
