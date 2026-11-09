#include "core_catalog.h"

namespace GUIWorker {

namespace {

const std::vector<CoreInfo> kCores = {
    {"vanilla", "Minecraft 原版（官方服务端）", "vanilla", false},
    {"paper", "Paper", "plugin", true},
    {"purpur", "Purpur", "plugin", true},
    {"folia", "Folia（多线程）", "plugin", true},
    {"spigot", "Spigot", "plugin", true},
    {"pufferfish", "Pufferfish", "plugin", true},
    {"fabric", "Fabric", "modded", false},
    {"quilt", "Quilt", "modded", false},
    {"forge", "Forge", "modded", false},
    {"neoforge", "NeoForge", "modded", false},
    {"mohist", "Mohist（Forge + Bukkit）", "hybrid", true},
    {"arclight", "Arclight（Forge/NeoForge + Bukkit）", "hybrid", true},
    {"cardboard", "Cardboard（Fabric + Bukkit）", "hybrid", true},
    {"velocity", "Velocity", "proxy", false},
    {"bungeecord", "BungeeCord", "proxy", false},
    {"waterfall", "Waterfall", "proxy", false},
    {"geyser", "Geyser（基岩版 Java 互通）", "geyser", false},
};

} // namespace

const std::vector<CoreInfo>& availableCores()
{
    return kCores;
}

std::string coreCategoryLabel(const std::string& category)
{
    if (category == "vanilla") return "原版";
    if (category == "plugin") return "插件服";
    if (category == "modded") return "模组服";
    if (category == "hybrid") return "混合服";
    if (category == "proxy") return "代理服";
    if (category == "geyser") return "基岩版互通";
    return category;
}

std::string coreDisplayName(const std::string& id)
{
    for (const auto& core : kCores) {
        if (core.id == id) {
            return core.name;
        }
    }
    return id;
}

} // namespace GUIWorker
