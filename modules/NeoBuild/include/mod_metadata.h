#pragma once

#include <string>
#include <vector>

namespace NeoBuild {

// 从 JAR 包提取 modId（加载器优先级: NeoForge > Forge mods.toml >
// Fabric fabric.mod.json > Forge 旧版 mcmod.info），失败返回空列表。
std::vector<std::string> extractModIds(const std::string& jarPath);

} // namespace NeoBuild
