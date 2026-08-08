#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <cancel_token.h>
#include <IBuildProgress.h>
#include <branch_merger.h>

namespace NeoBuild {

// 生成 U/M/D 虚拟构建预览结构（供构建清单页文件树展示）。
//   buildDir:  虚拟构建目录（已生成文件的中间目录）
//   targetDir: 目标工作目录（hmcl 真实比对基准；为空 = 不做比对，全部未更改）
//   progress/cancel: 可空指针，静默降级
// 返回 JSON 数组: [{"path","dir","umd"}]，umd ∈ {"", "U", "M", "D"}
//   U = 目标目录不存在该文件（新建）；M = 内容不同（修改）；D = 目标有而构建无（删除）；"" = 未更改
nlohmann::json generateUmdStructure(
    const std::string& buildDir,
    const std::string& targetDir,
    NeoCore::IBuildProgress* progress = nullptr,
    NeoCore::CancelToken* cancel = nullptr);

// 分支继承层叠合并版：buildSet 由 BranchLayer 链在内存中虚拟合并
// （层叠覆盖 + delete/override 标记应用，不落盘），供 IDE 输出树展示最终结果
nlohmann::json generateUmdStructureFromLayers(
    const std::vector<BranchLayer>& layers,
    const std::string& targetDir,
    NeoCore::IBuildProgress* progress = nullptr,
    NeoCore::CancelToken* cancel = nullptr);

} // namespace NeoBuild
