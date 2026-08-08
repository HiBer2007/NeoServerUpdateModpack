# NeoServer Update Modpack

NeoServer 更新整合包管理工具 — 基于 C++17 + Qt + nlohmann/json 构建。

## 技术栈

| 组件 | 用途 |
|------|------|
| **C++17** | 标准语言 |
| **Qt** (Widgets + Network) | 图形界面与网络请求 |
| **nlohmann/json** | JSON 数据解析与生成 |

## 项目结构

```
NeoServerUpdateModpack/
├── CMakeLists.txt          # 主构建配置
├── vcpkg.json              # vcpkg 依赖清单
├── conanfile.txt           # Conan 依赖清单 (备选)
├── .gitignore
├── README.md
├── src/                    # 源代码
│   ├── main.cpp            # 入口文件
│   └── ...
├── include/                # 公共头文件
├── resources/              # 资源文件 (图标, qss等)
└── build/                  # 构建输出 (gitignore)
```

## 构建方式

### 前置条件

- CMake >= 3.20
- 支持 C++17 的编译器 (MSVC 2022 / GCC 11+ / Clang 14+)
- Qt 5.15+ 或 Qt 6.x
- [vcpkg](https://github.com/microsoft/vcpkg) (推荐) 或 Conan

### 使用 vcpkg 构建 (推荐)

```bash
# 1. 安装 vcpkg 依赖
vcpkg install --triplet=x64-windows

# 2. 配置 & 构建
cmake -B build -S . ^
    -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build

# 3. 运行
.\build\Release\NeoServerUpdateModpack.exe
```

### 使用 Conan 构建 (备选)

```bash
# 1. 安装依赖
conan install . --output-folder=build --build=missing

# 2. 配置 & 构建
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
cmake --build build

# 3. 运行
.\build\Release\NeoServerUpdateModpack.exe
```

### 直接使用系统 Qt

```bash
# 确保 Qt 已安装且在 PATH 中
cmake -B build -S .
cmake --build build
```

## 功能概览 (开发计划)

- [ ] 解析服务器更新元数据 (JSON)
- [ ] 下载并管理整合包文件
- [ ] 图形化进度显示
- [ ] 配置文件管理
