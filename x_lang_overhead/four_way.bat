@echo off
rem ============================================================
rem  four_way.bat —— 多组对比（Windows），刻意写成最笨的 if/goto 风格：
rem  不用 for /f、不用 && 链、不在括号块里赋值。可读性换稳定性。
rem    1 纯血 C++  / 2 Cython / 3 PyPy / 4 纯 CPython / 5 numba
rem  用法：four_way.bat [nodes] [repeats]
rem ============================================================
setlocal

set PYPY=C:\Program Files\pypy3.12-v8.0.1-win64\pypy3.exe

set N=%1
if "%N%"=="" set N=131071
set R=%2
if "%R%"=="" set R=100

echo ### nodes=%N% repeats=%R%
echo.

echo --- 环境 ---
where g++ >nul 2>nul
if errorlevel 1 echo g++     : NOT FOUND
if not errorlevel 1 echo g++     : OK
where clang++ >nul 2>nul
if errorlevel 1 echo clang++ : NOT FOUND
if not errorlevel 1 echo clang++ : OK
where cl >nul 2>nul
if errorlevel 1 echo cl      : NOT FOUND (需要在 x64 Native Tools 窗口里跑)
if not errorlevel 1 echo cl      : OK
if exist "%PYPY%" echo pypy3   : OK
if not exist "%PYPY%" echo pypy3   : NOT FOUND
python -c "import sys" >nul 2>nul
if errorlevel 1 echo python  : NOT FOUND
if not errorlevel 1 python -c "import sys;print('python  :',sys.version.split()[0])"
python -c "import Cython" >nul 2>nul
if errorlevel 1 echo cython  : NOT FOUND
if not errorlevel 1 python -c "import Cython;print('cython  :',Cython.__version__)"
python -c "import numba" >nul 2>nul
if errorlevel 1 echo numba   : NOT FOUND
if not errorlevel 1 python -c "import numba;print('numba   :',numba.__version__)"
echo.

rem ---------- 1) C++ ----------
echo === 1/5 C++ (-O2) ===
set CXX=
where g++ >nul 2>nul
if not errorlevel 1 set CXX=g++
if "%CXX%"=="" goto try_clang
goto build_gnu
:try_clang
where clang++ >nul 2>nul
if not errorlevel 1 set CXX=clang++
if "%CXX%"=="" goto try_cl
goto build_gnu
:try_cl
where cl >nul 2>nul
if not errorlevel 1 set CXX=cl
if "%CXX%"=="" goto no_cxx

:build_gnu
if /i "%CXX%"=="cl" goto build_cl
echo 编译器: %CXX%
%CXX% -O2 -std=c++20 -I..\common bench_cpp.cpp -o bench_cpp.exe
goto run_cpp

:build_cl
echo 编译器: cl
cl /nologo /O2 /std:c++20 /EHsc /utf-8 /I..\common bench_cpp.cpp /Fe:bench_cpp.exe
goto run_cpp

:run_cpp
if not exist bench_cpp.exe goto cpp_fail
bench_cpp.exe %N% %R%
goto after_cpp
:cpp_fail
echo 编译没有产出 bench_cpp.exe
goto after_cpp
:no_cxx
echo 没有找到任何 C++ 编译器，跳过这一组
:after_cpp
echo.

rem ---------- 2) Cython ----------
echo === 2/5 Cython ===
python -c "import Cython" >nul 2>nul
if errorlevel 1 goto no_cython
python setup.py build_ext --inplace
if not exist bench_cython*.pyd goto cython_nopyd
python bench_cython_run.py %N% %R%
goto after_cython
:cython_nopyd
echo 没有产出 .pyd —— 多半是找不到 MSVC
goto after_cython
:no_cython
echo 缺 Cython，跳过
:after_cython
echo.

rem ---------- 3) PyPy ----------
echo === 3/5 PyPy3 (JIT) ===
if not exist "%PYPY%" goto pypy_alt
"%PYPY%" bench_py.py %N% %R%
goto after_pypy
:pypy_alt
where pypy3 >nul 2>nul
if errorlevel 1 echo 找不到 pypy3，改本文件顶部的 PYPY 路径
if not errorlevel 1 pypy3 bench_py.py %N% %R%
:after_pypy
echo.

rem ---------- 4) CPython ----------
echo === 4/5 CPython ===
python bench_py.py %N% %R%
echo.

rem ---------- 5) numba ----------
echo === 5/5 numba ===
python -c "import numba" >nul 2>nul
if errorlevel 1 echo 缺 numba，跳过
if not errorlevel 1 python bench_numba.py %N% %R%

endlocal
