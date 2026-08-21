# ================================================================
# NeoInstaller 静态构建脚本
#
# 构建流程:
#   1. 确保主项目已构建 (build/deploy/ 存在)
#   2. 使用 vcpkg x64-windows-static triplet 获取静态 Qt6
#   3. 独立配置 CMake (不继承根 CMake 的动态 Qt)
#   4. 编译 + 嵌入 build/deploy/ 所有文件 → 单文件安装程序
#
# 前置条件 (一次性):
#   vcpkg install qt6[core,widgets]:x64-windows-static
#
# 构建产物:
#   build_installer/NeoInstaller.exe  (静态链接, 无外部 Qt 依赖)
# ================================================================

param(
    [string]$BuildType = "Release",
    [switch]$SkipMainBuild,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== NeoInstaller Static Build ===" -ForegroundColor Cyan

# Step 1: Build main project if needed
$deployDir = Join-Path $ScriptDir "build\deploy"
if (-not $SkipMainBuild -or -not (Test-Path $deployDir)) {
    Write-Host "[1/3] Building main project to get deploy files..." -ForegroundColor Cyan

    $cmakeExe = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
    $vcvars = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat"

    $batch = @"
@echo off
call "$vcvars" >nul 2>&1
"$cmakeExe" "$ScriptDir" -B "$ScriptDir\build" -DCMAKE_TOOLCHAIN_FILE=H:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH=H:/Qt/6.11.1/msvc2022_64 -DCMAKE_BUILD_TYPE=Debug -G Ninja >nul 2>&1
"$cmakeExe" --build "$ScriptDir\build" >nul 2>&1
"@
    $batchFile = Join-Path $env:TEMP "build_main.bat"
    $batch | Out-File $batchFile -Encoding ASCII
    cmd /c $batchFile
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[WARN] Main build had issues, but continuing..." -ForegroundColor Yellow
    }
}

if (-not (Test-Path $deployDir)) {
    Write-Host "[ERROR] deploy directory not found: $deployDir" -ForegroundColor Red
    Write-Host "  Build the main project first, or ensure build/deploy/ exists" -ForegroundColor Yellow
    exit 1
}

$fileCount = (Get-ChildItem $deployDir -Recurse -File | Measure-Object).Count
Write-Host "  Deploy files found: $fileCount" -ForegroundColor Gray

# Step 2: Static Qt available?
Write-Host "[2/3] Checking static Qt..." -ForegroundColor Cyan

$vcpkgStaticDir = "H:/vcpkg/installed/x64-windows-static"
$qtConfig = Join-Path $vcpkgStaticDir "share/qt6/Qt6Config.cmake"

if (Test-Path $qtConfig) {
    Write-Host "  Static Qt found: $vcpkgStaticDir" -ForegroundColor Green
    $qtPrefix = $vcpkgStaticDir
} else {
    Write-Host "  Static Qt NOT found." -ForegroundColor Yellow
    Write-Host "  Installing via vcpkg (this will take 30-60 minutes)..." -ForegroundColor Yellow
    Write-Host "  Run: vcpkg install qt6[core,widgets]:x64-windows-static" -ForegroundColor White

    vcpkg install qt6[core,widgets]:x64-windows-static
    if ($LASTEXITCODE -ne 0) {
        Write-Host "[ERROR] vcpkg static Qt install failed" -ForegroundColor Red
        exit 1
    }
    $qtPrefix = $vcpkgStaticDir
}

# Step 3: Build installer with static Qt
Write-Host "[3/3] Building NeoInstaller (static Qt)..." -ForegroundColor Cyan

$installerSrc = Join-Path $ScriptDir "modules\NeoInstaller"
$buildDir = Join-Path $ScriptDir "build_installer"

if ($Clean -and (Test-Path $buildDir)) {
    Remove-Item -Recurse -Force $buildDir
}

$cmakeArgs = @(
    "-S", $installerSrc,
    "-B", $buildDir,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=$BuildType",
    "-DCMAKE_TOOLCHAIN_FILE=H:/vcpkg/scripts/buildsystems/vcpkg.cmake",
    "-DVCPKG_TARGET_TRIPLET=x64-windows-static",
    "-DVCPKG_HOST_TRIPLET=x64-windows-static",
    "-DBUILD_DEPLOY_DIR=$deployDir"
)

$cmakeExe = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe"
$vcvars = "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Auxiliary/Build/vcvars64.bat"

$batch = @"
@echo off
call "$vcvars" >nul 2>&1
"$cmakeExe" $($cmakeArgs -join ' ') 2>&1
if %ERRORLEVEL% NEQ 0 exit /b 1
"$cmakeExe" --build "$buildDir" --config $BuildType 2>&1
"@
$batchFile = Join-Path $env:TEMP "build_inst.bat"
$batch | Out-File $batchFile -Encoding ASCII
cmd /c $batchFile

if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Installer build failed" -ForegroundColor Red
    exit 1
}

# Verify
$exe = Get-ChildItem $buildDir -Recurse -Filter "NeoInstaller.exe" | Select-Object -First 1
if ($exe) {
    $sizeMB = [math]::Round($exe.Length / 1MB, 2)
    Write-Host ""
    Write-Host "=== BUILD SUCCESS ===" -ForegroundColor Green
    Write-Host "  Output: $($exe.FullName)" -ForegroundColor White
    Write-Host "  Size:   $sizeMB MB ($($exe.Length) bytes)" -ForegroundColor White
    Write-Host "  Status: STATIC Qt + $fileCount embedded files" -ForegroundColor Green
    Write-Host "  Run:    .\$($exe.Name)" -ForegroundColor Gray

    if ($sizeMB -lt 3) {
        Write-Host "  WARNING: File size < 3MB suggests Qt is NOT statically linked" -ForegroundColor Yellow
        Write-Host "  Expected size: 15-25MB for static Qt + embedded files" -ForegroundColor Yellow
        Write-Host "  Check that CMAKE_PREFIX_PATH points to static Qt build" -ForegroundColor Yellow
    }
} else {
    Write-Host "[ERROR] NeoInstaller.exe not found" -ForegroundColor Red
}
