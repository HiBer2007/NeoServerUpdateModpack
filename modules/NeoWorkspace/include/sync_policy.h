#pragma once

#include <string>
#include <vector>

namespace NeoWorkspace {

// L2 配置文件特化（sync_policies.files）：
//   mode: full    = 仅覆盖（直接写入目标）
//         partial = 半同步 merge（经 IConfigParser，tracked_keys/tracked_lines 参与）
//         ignore  = 忽略（不写目标）
struct SyncPolicyFile {
    std::string path;
    std::string mode;
    std::vector<std::string> trackedKeys;
    std::vector<int> trackedLines;
};

// L1 文件夹策略（sync_policies.folders，作用于任意文件夹，递归含子文件夹）：
//   policy: skip                 = 跳过
//           mirror               = 覆盖（严格镜像：清空重写 + 删除多余项）
//           incremental_add      = 增量补充（只补缺失，已存在的文件不动）
//           incremental_overwrite= 增量覆盖（保留多余项，被改过也写入）
//           default              = 兜底（使用分支/顶层 default_folder_policy）
struct SyncPolicyFolder {
    std::string path;
    std::string policy;
};

// 有效同步策略（顶层 sync_policies 与分支级 sync_policies 合并后的结果）
struct SyncPolicy {
    std::string defaultFolderPolicy = "incremental_add";
    std::vector<SyncPolicyFolder> folders;
    std::vector<SyncPolicyFile> files;
};

} // namespace NeoWorkspace
