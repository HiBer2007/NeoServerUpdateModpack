# nbtcpp — Minecraft NBT / Region / SNBT Library for C++

[![License: LGPLv3](https://img.shields.io/badge/License-LGPLv3-blue.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![zlib](https://img.shields.io/badge/dependency-zlib-green.svg)](https://zlib.net/)

**nbtcpp** 是一个从 C# [fNbt](https://github.com/fragmer/fNbt) + [NbtStudio](https://github.com/tryashtar/nbt-studio) 移植到 C++17 的完整 NBT 库。支持：

- ✅ **NBT 二进制格式** — 读取/写入 Minecraft Java Edition（大端序）和 Bedrock Edition（小端序）
- ✅ **标签层次** — Byte, Short, Int, Long, Float, Double, String, ByteArray, IntArray, LongArray, List, Compound
- ✅ **流式解析** — 前向只读 NbtReader 和流式写入 NbtWriter（无需构建完整树）
- ✅ **压缩** — GZip、ZLib 和未压缩格式，自动检测
- ✅ **MCA Region 文件** — Minecraft Anvil 区域文件（.mca / .mcr），包含 32×32 块表
- ✅ **SNBT** — String NBT 解析与序列化，支持缩进/JSON 风格/预览等多种格式
- ✅ **Diff/Patch** — 树级语义差异生成与还原，处理无序 Compound，流式写出，恒定内存
- ✅ **现代 C++** — 使用 C++17 标准库（std::optional, std::variant, 智能指针）
- ✅ **文档注释** — 全 Doxygen 风格文档，覆盖所有公开 API

## 目录结构

```
nbtcpp/
├── CMakeLists.txt              # CMake 构建文件
├── README.md                   # 本文件
├── lib/zlib/                   # 内置 zlib 源码（无需网络下载）
├── include/nbtcpp/             # 公共头文件
│   ├── nbt_tag_type.h          # NbtTagType 枚举
│   ├── nbt_compression.h       # NbtCompression 枚举
│   ├── nbt_exception.h         # 异常类型
│   ├── endian_utils.h          # 端序工具
│   ├── nbt_binary_reader.h     # 二进制读取器
│   ├── nbt_binary_writer.h     # 二进制写入器
│   ├── nbt_reader.h            # 流式解析器
│   ├── nbt_writer.h            # 流式写入器
│   ├── nbt_file.h              # 文件级 I/O
│   ├── region_file.h           # MCA 区域文件
│   ├── chunk.h                 # Chunk 数据
│   ├── nbt_diff.h              # Diff/Patch 差异库
│   ├── tags/                   # 标签类
│   │   ├── nbt_tag.h           #   基类
│   │   ├── nbt_container_tag.h #   容器基类
│   │   ├── nbt_byte.h          #   TAG_Byte
│   │   ├── nbt_short.h         #   TAG_Short
│   │   ├── nbt_int.h           #   TAG_Int
│   │   ├── nbt_long.h          #   TAG_Long
│   │   ├── nbt_float.h         #   TAG_Float
│   │   ├── nbt_double.h        #   TAG_Double
│   │   ├── nbt_string.h        #   TAG_String
│   │   ├── nbt_byte_array.h    #   TAG_Byte_Array
│   │   ├── nbt_int_array.h     #   TAG_Int_Array
│   │   ├── nbt_long_array.h    #   TAG_Long_Array
│   │   ├── nbt_list.h          #   TAG_List
│   │   └── nbt_compound.h      #   TAG_Compound
│   └── snbt/                   # SNBT 支持
│       ├── snbt_parser.h       #   SNBT 解析器
│       └── snbt_maker.h        #   SNBT 序列化器
├── src/                        # 实现文件
│   ├── nbt_binary_reader.cpp
│   ├── nbt_binary_writer.cpp
│   ├── nbt_reader.cpp
│   ├── nbt_writer.cpp
│   ├── nbt_file.cpp
│   ├── region_file.cpp
│   ├── chunk.cpp
│   ├── nbt_diff.cpp             # Diff/Patch 实现
│   ├── tags/
│   └── snbt/
└── examples/                   # 示例
    ├── example_main.cpp        # 综合示例
    └── example_region.cpp      # Region 文件示例
```

## 构建

### 依赖

- **CMake** ≥ 3.14
- **C++17** 编译器（GCC 8+, Clang 7+, MSVC 2019+）
- **zlib** — **已内置**在 `lib/zlib/` 中，无需单独安装

### 使用 CMake

```bash
cd nbtcpp
mkdir build && cd build

# 配置（Release 模式）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 构建库 + 示例
cmake --build . --config Release

# 运行示例
./examples/Release/nbtcpp_example       # Windows
# ./examples/nbtcpp_example             # Linux/macOS
```

### 作为子项目集成到你的 CMake 项目

```cmake
add_subdirectory(path/to/nbtcpp)
target_link_libraries(your_app PRIVATE nbtcpp)
target_include_directories(your_app PRIVATE path/to/nbtcpp/include)
```

**注意**：`nbtcpp` 会使用 `add_subdirectory(lib/zlib)` 自动编译内置的 zlib。如果希望使用系统安装的 zlib，可以删除/替换 `lib/zlib/` 目录并在调用 `add_subdirectory` 之前添加：
```cmake
find_package(ZLIB REQUIRED)
```

## 快速开始

### 构建 NBT 树

```cpp
#include "nbtcpp/tags/nbt_compound.h"
#include "nbtcpp/tags/nbt_int.h"
#include "nbtcpp/tags/nbt_string.h"
#include "nbtcpp/tags/nbt_list.h"

using namespace nbtcpp;

// 创建根 Compound
auto root = std::make_shared<NbtCompound>("Level");

// 添加标量标签
root->add(std::make_unique<NbtInt>("x", 100));
root->add(std::make_unique<NbtInt>("y", 64));
root->add(std::make_unique<NbtInt>("z", -200));
root->add(std::make_unique<NbtString>("Biome", "plains"));

// 添加嵌套 Compound
auto nested = std::make_unique<NbtCompound>("nested");
nested->add(std::make_unique<NbtInt>("a", 1));
nested->add(std::make_unique<NbtInt>("b", 2));
root->add(std::move(nested));

// 添加 List
auto list = std::make_unique<NbtList>("list");
list->add(std::make_unique<NbtString>("item1"));
list->add(std::make_unique<NbtString>("item2"));
root->add(std::move(list));
```

### 读写 NBT 文件

```cpp
#include "nbtcpp/nbt_file.h"

// 写入文件
NbtFile file(root);
file.save_to_file("output.nbt", NbtCompression::ZLib);   // ZLib 压缩
file.save_to_file("output.nbt", NbtCompression::GZip);   // GZip 压缩
file.save_to_file("output.nbt", NbtCompression::None);   // 未压缩

// 读取文件
NbtFile loaded("input.nbt");
auto tag = loaded.root_tag<NbtCompound>();

// 读取基岩版小端序文件
NbtFile bedrock_file;
bedrock_file.set_big_endian(false);  // 小端序
bedrock_file.load_from_file("bedrock.dat", NbtCompression::AutoDetect);
```

### 流式解析（无需构建完整树）

```cpp
#include "nbtcpp/nbt_reader.h"

std::ifstream file("data.nbt", std::ios::binary);
NbtReader reader(file, true);  // big-endian

while (reader.read_to_following()) {
    std::cout << reader.tag_name() << ": "
              << to_string(reader.tag_type()) << "\n";
    if (reader.has_value()) reader.skip_value();
}
```

### 流式写入（无需构建完整树）

```cpp
#include "nbtcpp/nbt_writer.h"

std::ofstream file("output.nbt", std::ios::binary);
NbtWriter writer(file, "Root", true);  // big-endian

writer.begin_compound("pos");
writer.write_int("x", 10);
writer.write_int("y", 64);
writer.write_int("z", -128);
writer.end_compound();

writer.write_string("name", "test");
writer.finish();  // 自动关闭所有打开的标签
```

### MCA Region 文件

```cpp
#include "nbtcpp/region_file.h"
#include "nbtcpp/chunk.h"

// 打开区域文件
RegionFile region("r.0.0.mca");
std::cout << "Chunks: " << region.chunk_count() << "\n";

// 访问特定 Chunk
Chunk* chunk = region.get_chunk(0, 0);
if (chunk) {
    chunk->load();  // 惰性加载
    auto data = chunk->data();
    if (data) {
        auto* level = data->get_as<NbtCompound>("Level");
        auto* x = level->get_as<NbtInt>("xPos");
        // ...
    }
}
```

### SNBT

```cpp
#include "nbtcpp/snbt/snbt_parser.h"
#include "nbtcpp/snbt/snbt_maker.h"

// 解析 SNBT
auto tag = snbt::parse("{x: 1, y: 2, name: \"hello\"}");

// 序列化为 SNBT
std::string text = snbt::to_snbt(*tag, snbt::Options::default_options());

// 漂亮的格式化输出
std::string pretty = snbt::to_snbt(*tag, snbt::Options::default_expanded());

// JSON 风格
std::string json = snbt::to_snbt(*tag, snbt::Options::json_like());
```

### Diff/Patch（语义差异与还原）

```cpp
#include "nbtcpp/nbt_diff.h"

// 生成差异文件（流式，不缓存全部差异到内存）
save_diff_file("diff.bin", *treeA.root_tag(), *treeB.root_tag());

// 应用差异文件（原地修改 treeA，使其变为 treeB）
apply_diff_file("diff.bin", *treeA.root_tag());
// treeA 现在等于 treeB

// 流式回调模式（自定义处理每个差异）
diff_subtrees(*treeA.root_tag(), *treeB.root_tag(),
    [](const std::string& path, const std::vector<std::string>& segments,
       DiffOp op, const NbtTag* value) {
        if (op == DiffOp::Set)
            std::cout << "SET   " << path << std::endl;
        else
            std::cout << "REMOVE " << path << std::endl;
    });
```

**特性：**
- **语义比较**：Compound 按名字匹配子节点（忽略顺序），List 按位置比较
- **O(n) 双指针归并**：利用 `std::map` 的有序性，一次遍历完成比较
- **流式写出**：差异发现时立即写入文件，内存占用仅与树深度成正比
- **分段路径**：路径用长度前缀存储每段，正确处理标签名中的点号（如 `"Bukkit.Version"`）
- **深度排序 apply**：REMOVE 深优先（先删叶子），SET 浅优先（先建父节点）

**差异文件格式**

紧凑二进制格式（v2），每条目：
```
[1B op][2B seg_count][for each: 2B seg_len + seg_bytes][4B value_len + value_bytes]
```

## NBT 格式参考

| 类型 | ID | 载荷 | 说明 |
|---|---|---|---|
| `TAG_End` | 0 | 无 | 标记 Compound 结束 |
| `TAG_Byte` | 1 | 1 字节 | 有符号/无符号字节 |
| `TAG_Short` | 2 | 2 字节 | 有符号 16 位整数 |
| `TAG_Int` | 3 | 4 字节 | 有符号 32 位整数 |
| `TAG_Long` | 4 | 8 字节 | 有符号 64 位整数 |
| `TAG_Float` | 5 | 4 字节 | IEEE-754 单精度浮点数 |
| `TAG_Double` | 6 | 8 字节 | IEEE-754 双精度浮点数 |
| `TAG_Byte_Array` | 7 | [4B 长度] + 字节 | 字节数组 |
| `TAG_String` | 8 | [2B 长度] + UTF-8 | UTF-8 字符串 |
| `TAG_List` | 9 | [1B 元素类型][4B 长度] + 元素 | 同类型元素列表 |
| `TAG_Compound` | 10 | 标签序列以 TAG_End 结束 | 命名标签的容器 |
| `TAG_Int_Array` | 11 | [4B 长度] + 4B×N | 32 位整数数组 |
| `TAG_Long_Array` | 12 | [4B 长度] + 8B×N | 64 位整数数组 |

所有多字节值在 Java Edition 中使用**大端序**（big-endian），在 Bedrock Edition 中使用**小端序**（little-endian）。

## Region 文件格式参考

```
┌──────────────────────────────┐
│  Location Table (4096 bytes) │  ← 4 字节/槽，32×32=1024 槽
│  ┌──────────────────────────┐│
│  │ [3B sector offset][1B    ││
│  │  sector count]           ││
│  └──────────────────────────┘│
├──────────────────────────────┤
│  Timestamp Table (4096 bytes)│  ← 4 字节 UNIX 时间戳/槽
├──────────────────────────────┤
│  Chunk Data (sector-aligned) │  ← 每个块: [4B 长度][1B 压缩类型][数据]
│  ┌──────────────────────────┐│
│  │ [length: int32 BE]       ││
│  │ [compression: byte]      ││
│  │ [zlib-compressed NBT]    ││
│  └──────────────────────────┘│
└──────────────────────────────┘
```

压缩类型: 1 = GZip, 2 = ZLib, 3 = 未压缩, 128+ = 外部块

## 移植来源

本项目是以下 C# 项目的 C++ 移植：

- **[fNbt](https://github.com/fragmer/fNbt)** — C# NBT 库（核心引擎）
- **[NbtStudio](https://github.com/tryashtar/nbt-studio)** — NBT 编辑器（Region/Chunk/SNBT）

## 许可证

GNU Lesser General Public License v3.0 — 参见 [LICENSE](LICENSE) 文件。
