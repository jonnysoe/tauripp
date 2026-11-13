@echo off
setlocal

call :setCompiler %*

:: Call vcvars if cl is not in PATH
call :updatePath || echo Cannot run %~nx0 without MSVC! && exit /b %ERRORLEVEL%

pushd %~dp0
call :invokeCmake
set ret=%ERRORLEVEL%
popd

exit /b %ret%

::======================================================================
:: Set CMake compiler variables based on input args
::======================================================================
:setCompiler
:: Check if input args have msvc or cl
:: NOTE: they need to be in order, otherwise clang-cl will be detected as clang, and clang will be detected as cl, etc.
echo %* | findstr /i "clang-cl" >nul 2>&1 && goto :setClangCl
echo %* | findstr /i "clang" >nul 2>&1 && goto :setClang
echo %* | findstr /i "cl" >nul 2>&1 && goto :setMsvc
echo %* | findstr /i "msvc" >nul 2>&1 && goto :setMsvc

:: Set default compiler to clang if available, otherwise cl
:: @todo these seems correct, however someone might set CMAKE_CXX_COMPILER to cl without setting CMAKE_C_COMPILER
::       then the first check will override it to clang, fix it another time
if not defined CMAKE_C_COMPILER where clang >nul 2>&1 && goto :setClang
if not defined CMAKE_CXX_COMPILER where clang++ >nul 2>&1 && goto :setClang
if not defined CMAKE_C_COMPILER goto :setMsvc
if not defined CMAKE_CXX_COMPILER goto :setMsvc
exit /b 0

:setMsvc
set CMAKE_C_COMPILER=cl
set CMAKE_CXX_COMPILER=cl
exit /b 0

:setClang
set CMAKE_C_COMPILER=clang
set CMAKE_CXX_COMPILER=clang++
exit /b 0

:setClangCl
set CMAKE_C_COMPILER=clang-cl
set CMAKE_CXX_COMPILER=clang-cl
exit /b 0

::======================================================================
:: CMake wrapper
::======================================================================
:invokeCmake
:: Set Ninja generator if ninja is in PATH
if not defined CMAKE_GENERATOR where ninja >nul 2>&1 && set CMAKE_GENERATOR=Ninja Multi-Config

:: Make sure the build directory is clean if the generator has changed or if CMakeCache.txt does not exist
if not exist build\CMakeCache.txt cmake -E rm -rf build
if exist build\CMakeCache.txt findstr "CMAKE_GENERATOR" build\CMakeCache.txt | findstr /E "%CMAKE_GENERATOR%" >nul 2>&1 || cmake -E rm -rf build
::cmake -E rm -rf build

:: Invoke CMake's configure-generate step
if not exist build cmake -S . -B build || exit /b %ERRORLEVEL%

:: Invoke CMake's build step
cmake --build build --config Release
exit /b %ERRORLEVEL%

::======================================================================
:: Check if cl is in PATH, set it if available, error exit if not
::======================================================================
:updatePath
:: Check if CMAKE_C_COMPILER and CMAKE_CXX_COMPILER are set to cl, if not, we exit
if "%CMAKE_C_COMPILER%" neq "cl" exit /b 0
if "%CMAKE_CXX_COMPILER%" neq "cl" exit /b 0

:: exit if cl is already in PATH
where cl >nul 2>&1 && exit /b %ERRORLEVEL%

:: Get vswhere path
for /f "tokens=* USEBACKQ" %%A in (`dir /s /b "%ProgramFiles(x86)%\*vswhere.exe"`) do set VSWHERE=%%A
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

:: Get latest VS installation path
for /f "tokens=* USEBACKQ" %%A in (`call "%VSWHERE%" -latest -property installationPath`) do set VS_PATH=%%A
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

:: Get vcvarsall.bat path
for /f "tokens=* USEBACKQ" %%A in (`dir /s /b "%VS_PATH%\*vcvarsall.bat"`) do set VCVARS=%%A
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

:: Call vcvarsall.bat to set up environment variables for MSVC toolchain
endlocal
call "%VCVARS%" %PROCESSOR_ARCHITECTURE%
setlocal

:: However vcvarsall.bat does not set error code, so we set it by checking if cl is in PATH after calling it
where cl >nul 2>&1
exit /b %ERRORLEVEL%
