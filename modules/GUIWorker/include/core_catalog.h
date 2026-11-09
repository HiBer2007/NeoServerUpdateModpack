#pragma once

#include <string>
#include <vector>

namespace GUIWorker {

struct CoreInfo {
    std::string id;
    std::string name;
    std::string category;
    bool supportsPlugins = false;
};

const std::vector<CoreInfo>& availableCores();
std::string coreCategoryLabel(const std::string& category);
std::string coreDisplayName(const std::string& id);

} // namespace GUIWorker
