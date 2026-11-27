# 一键配置并编译（Qt6 + MinGW + CMake/Ninja）
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $root

$qt = $env:QT_DIR
if (-not $qt) { $qt = "C:/Qt/6.7.3/mingw_64" }
$mingw = $env:MINGW_DIR
if (-not $mingw) { $mingw = "C:/Qt/Tools/mingw1310_64" }

# 优先使用项目根目录 .venv 里的 cmake/ninja，否则用系统 PATH 中的
$venvTools = Join-Path $projectRoot ".venv/Scripts"
$cmake = "cmake"
$ninja = "ninja"
if (Test-Path (Join-Path $venvTools "cmake.exe")) { $cmake = Join-Path $venvTools "cmake.exe" }
if (Test-Path (Join-Path $venvTools "ninja.exe")) { $ninja = Join-Path $venvTools "ninja.exe" }

$env:Path = "$qt/bin;$mingw/bin;" + $env:Path

& $cmake -S $root -B "$root/build" -G Ninja `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_PREFIX_PATH=$qt" `
    "-DCMAKE_C_COMPILER=$mingw/bin/gcc.exe" `
    "-DCMAKE_CXX_COMPILER=$mingw/bin/g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$ninja"

& $cmake --build "$root/build"

Write-Host "构建完成: $root/build/MeetingAI.exe"
