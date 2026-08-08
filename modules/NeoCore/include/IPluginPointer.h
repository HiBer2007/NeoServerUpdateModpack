#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace NeoCore {

struct PointerInfo {
    std::string resolver;
    std::string sha256;
    nlohmann::json metadata;
};

struct PointerFileData {
    std::string sha256;
    std::vector<std::string> original_names;
    std::vector<PointerInfo> resolvers;
    std::vector<std::string> download_methods;

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["sha256"] = sha256;
        nlohmann::json names = nlohmann::json::array();
        for (auto& n : original_names) names.push_back(n);
        j["original_names"] = names;
        nlohmann::json res = nlohmann::json::array();
        for (auto& r : resolvers) {
            nlohmann::json entry;
            entry["resolver"] = r.resolver;
            entry["metadata"] = r.metadata;
            res.push_back(entry);
        }
        j["resolvers"] = res;
        nlohmann::json methods = nlohmann::json::array();
        for (auto& m : download_methods) methods.push_back(m);
        j["download_methods"] = methods;
        return j;
    }

    static PointerFileData fromJson(const nlohmann::json& j) {
        PointerFileData pfd;
        pfd.sha256 = j.value("sha256", "");
        if (j.contains("original_names") && j["original_names"].is_array()) {
            for (auto& n : j["original_names"])
                if (n.is_string()) pfd.original_names.push_back(n.get<std::string>());
        }
        if (j.contains("resolvers") && j["resolvers"].is_array()) {
            for (auto& r : j["resolvers"]) {
                PointerInfo pi;
                pi.sha256 = pfd.sha256;
                pi.resolver = r.value("resolver", "");
                pi.metadata = r.value("metadata", nlohmann::json::object());
                pfd.resolvers.push_back(pi);
            }
        }
        if (j.contains("download_methods") && j["download_methods"].is_array()) {
            for (auto& m : j["download_methods"])
                if (m.is_string()) pfd.download_methods.push_back(m.get<std::string>());
        }
        return pfd;
    }
};

class IPluginPointer {
public:
    virtual ~IPluginPointer() = default;

    virtual std::string name() const = 0;

    virtual bool can_handle(const PointerInfo& ptr) const = 0;

    virtual std::string resolve_url(const PointerInfo& ptr) = 0;

    virtual bool validate(
        const std::string& filepath,
        const std::string& expected_sha256) = 0;

    virtual std::vector<std::string> supported_download_methods() const { return {}; }

    virtual bool can_batch_search() const { return false; }

    virtual std::vector<PointerInfo> batch_search(
        const std::string& modId, const std::string& version,
        const std::string& sha256) { return {}; }
};

class IDownloadMethod {
public:
    virtual ~IDownloadMethod() = default;

    virtual std::string name() const = 0;

    virtual bool download(
        const std::string& url,
        const std::string& destPath,
        const std::string& expectedSha256) = 0;
};

using CreatePointerFunc = IPluginPointer* (*)();
using CreateDownloaderFunc = IDownloadMethod* (*)();

} // namespace NeoCore
