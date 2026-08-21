# NeoInstaller 静态链接 Qt 构建指南

## 参考资料

- Qt 官方 Windows 部署文档：https://doc.qt.io/qt-6/windows-deployment.html
- Qt 官方静态链接文档：https://doc.qt.io/qt-6/windows-deployment.html#static-linking
- 中文镜像：https://doc.qt.ac.cn/qt-6/macos-deployment.html#static-linking
- MSYS2 静态 Qt 包：https://packages.msys2.org/packages/mingw-w64-x86_64-qt6-static

## 原理

NeoInstaller 是安装程序，必须作为**单个自包含可执行文件**分发（用户双击即可运行，无需安装任何依赖）。

Qt 官方静态链接方法：
```
cd C:\path\to\Qt
configure -static -release -prefix H:/Qt-static/6.11.1/msvc2022_64 -nomake examples -nomake tests
cmake --build . --parallel
cmake --install .
```

应用链接到静态 Qt（CMake）：
```cmake
cmake_minimum_required(VERSION 3.21.1)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_add_executable(NeoInstaller WIN32 ...)
```

关键区别：
- 动态 Qt：`add_executable` → exe 依赖 Qt DLL（需 windeployqt）
- 静态 Qt：`qt_add_executable` + `qt_import_plugins` → exe 自包含

## 操作步骤

### 1. 构建静态 Qt

本项目已安装 Qt 6.11.1 动态版本于 `H:\Qt\6.11.1\msvc2022_64`，源码位于 `H:\Qt\6.11.1\Src`。

```powershell
# 初始化 MSVC 环境
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"'

# 进入构建目录
mkdir H:\Qt\6.11.1\Src\build_static
cd H:\Qt\6.11.1\Src\build_static

# 配置静态构建（约 5 分钟）
..\configure -static -release -prefix H:/Qt-static/6.11.1/msvc2022_64 -nomake examples -nomake tests -submodules qtbase

# 编译（约 30 分钟，取决于 CPU 核心数）
cmake --build . --parallel

# 安装到目标路径
cmake --install .
```

构建产物位于 `H:\Qt-static\6.11.1\msvc2022_64`：
- `lib/Qt6Core.lib`（~37MB）
- `lib/Qt6Widgets.lib`（~36MB）
- `lib/Qt6Gui.lib`
- `lib/cmake/Qt6/Qt6Config.cmake`
- 等共 23 个静态库

### 2. CMake 预设配置

在 `CMakePresets.json` 中配置 `installer-static` 预设：

```json
{
    "name": "installer-static",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/build_installer",
    "cacheVariables": {
        "CMAKE_C_COMPILER": "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe",
        "CMAKE_CXX_COMPILER": "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64/cl.exe",
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_RC_COMPILER": "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe",
        "CMAKE_MT": "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/mt.exe",
        "CMAKE_PREFIX_PATH": "H:/Qt-static/6.11.1/msvc2022_64",
        "INSTALLER_ONLY_BUILD": "ON",
        "DEPLOY_SOURCE": "${sourceDir}/build/deploy"
    },
    "environment": {
        "INCLUDE": "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/include;...",
        "LIB": "C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools/VC/Tools/MSVC/14.51.36231/lib/x64;..."
    }
}
```

关键配置项：
| 变量 | 值 | 说明 |
|------|-----|------|
| `CMAKE_PREFIX_PATH` | `H:/Qt-static/6.11.1/msvc2022_64` | 指向静态 Qt 安装 |
| `INSTALLER_ONLY_BUILD` | `ON` | 跳过主程序依赖（vcpkg 包等） |
| `DEPLOY_SOURCE` | `${sourceDir}/build/deploy` | 嵌入文件的来源目录 |

### 3. 构建流程

```
┌─────────────────────────────┐
│ 1. msvc 预设构建主程序       │
│    → build/deploy/*.exe     │  ← 动态Qt，这是"货物"
│    → build/deploy/*.dll     │
│    → build/deploy/parsers/  │
└─────────────────────────────┘
              │
              │ DEPLOY_SOURCE 指向这里
              ▼
┌─────────────────────────────┐
│ 2. installer-static 预设    │
│    静态Qt构建安装程序自身    │  ← 静态Qt，这是"运输工具"
│    把 build/deploy/ 全部文件  │
│    嵌入到 QRC 资源中         │
│    → NeoInstaller.exe       │
└─────────────────────────────┘
```

在 VS Code 中：
1. 选择 `msvc` 预设 → 构建主程序
2. 选择 `installer-static` 预设 → 配置 + 构建 Installer

### 4. 安装程序工作原理

1. Installer 自身用静态 Qt 编译，无外部 Qt DLL 依赖
2. 构建时 CMake 扫描 `build/deploy/` 生成 `installer.qrc`
3. Qt RCC 编译器将全部文件嵌入二进制
4. 运行时通过 `QDirIterator(":/deploy/")` 遍历资源并释放到目标目录
5. 写入 `install.conf` 记录安装配置（不使用注册表）

### 5. 常见问题

**Q: 构建脚本显示 "deploy/ not found"**
A: 先用 `msvc` 预设构建主程序，确保 `build/deploy/` 存在。

**Q: 链接时 CRT 不匹配（LNK2038: MT_StaticRelease vs MD_DynamicRelease）**
A: 静态 Qt 构建时用的 CRT 要与 Installer 匹配。检查 `configure` 是否加了 `-static-runtime`。若无，Installer 的 CMakeLists 不要设置 `MSVC_RUNTIME_LIBRARY`。

**Q: 运行时释放的文件混入 Qt 内部资源**
A: QRC 前缀必须用独立路径（`/deploy/`），且提取代码只读此路径。

**Q: 安装程序需要管理员权限**
A: 若安装到 `C:\Program Files\`，需右键"以管理员身份运行"或设置 `requestedExecutionLevel`。

### 6. NeoInstaller CMakeLists 核心逻辑

```cmake
# 检测静态 Qt
set(IS_STATIC_QT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES "static" OR MINGW)
    set(IS_STATIC_QT ON)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 生成 QRC 嵌入 deploy 文件
file(WRITE ${QRC_OUT} "<RCC>\n  <qresource prefix=\"/deploy/\">\n")
file(GLOB_RECURSE EMBED_FILES LIST_DIRECTORIES false "${DEPLOY_SOURCE}/*")
foreach(f IN LISTS EMBED_FILES)
    file(RELATIVE_PATH rel "${DEPLOY_SOURCE}" "${f}")
    file(APPEND ${QRC_OUT} "    <file alias=\"${rel}\">${f}</file>\n")
endforeach()
file(APPEND ${QRC_OUT} "  </qresource>\n</RCC>\n")

# 使用 qt_add_executable（静态插件导入）
qt_add_executable(NeoInstaller WIN32 ${INSTALLER_SOURCES} ${QRC_OUT})
target_link_libraries(NeoInstaller PRIVATE Qt6::Core Qt6::Widgets)

if(IS_STATIC_QT AND MSVC)
    qt_import_plugins(NeoInstaller INCLUDE Qt::QWindowsIntegrationPlugin)
endif()
```
