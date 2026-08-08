#pragma once

#include "IConfigParser.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace NeoCore {

class PluginLoader {
public:
    PluginLoader();
    ~PluginLoader();

    void ScanDirectory(const std::string& parsersDir);

    IConfigParser* FindParser(const std::string& filepath) const;

    std::vector<ParserCapability> ListParsers() const;

    size_t ParserCount() const { return owned_parsers_.size(); }

private:
    struct LoadedParser {
        std::unique_ptr<IConfigParser> instance;
        ParserCapability capability;
        void* handle;
    };

    bool LoadPlugin(const std::string& dllPath, const std::string& metaPath);
    void RegisterParser(LoadedParser&& parser);

    std::unordered_map<std::string, IConfigParser*> registry_;
    std::vector<LoadedParser> owned_parsers_;
};

} // namespace NeoCore
