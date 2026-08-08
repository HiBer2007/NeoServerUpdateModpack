#pragma once

#include <string>
#include <vector>
#include <IBuildProgress.h>
#include <cancel_token.h>
#include <sync_policy.h>

namespace NeoBuild {

// 同步策略执行器：将中间构建目录（source）按 workspace.json 同步策略
// 同步到目标工作目录（target，HMCL 游戏目录）。
//
// 层次：
//   L1 文件夹策略（sync_policies.folders，最长前缀匹配，递归含子文件夹）：
//     skip / mirror（严格镜像：清空重写+删多余）/ incremental_add（只补缺失）
//     / incremental_overwrite（保留多余项，被改过也写入）/ default（兜底）
//   L2 配置文件特化（sync_policies.files）：full（仅覆盖）/ partial（半同步
//     merge，经 IConfigParser）/ ignore（忽略）
//   mods 目录为 mirror 时特殊处理：.disabled 清除、custom 模组收集到
//     .NSUM/custom/mod（只增不覆盖）、同步后复制回 mods、modId 冲突检测
//     （保留镜像模组，冲突模组留在库中 + 警告）。
//   .NSUM/hashes.json 记录每次写入文件的源 SHA-256，供增量策略比对。
class SyncPolicyExecutor {
public:
    struct Result {
        bool success = true;
        int copiedFiles = 0;
        int mergedFiles = 0;
        int skippedFiles = 0;
        int deletedFiles = 0;
        int customHarvested = 0;
        int customRestored = 0;
        std::vector<std::string> warnings;
    };

    Result execute(
        const std::string& sourceDir,
        const std::string& targetDir,
        const NeoWorkspace::SyncPolicy& policy,
        NeoCore::IBuildProgress* progress = nullptr,
        NeoCore::CancelToken* cancel = nullptr);
};

} // namespace NeoBuild
