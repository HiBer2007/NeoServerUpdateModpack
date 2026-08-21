#pragma once

#include <QWidget>
#include <QJsonObject>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace NeoCore {

class IConfigEditorExtension {
public:
    virtual ~IConfigEditorExtension() = default;

    virtual std::string fileExtension() const = 0;

    virtual QWidget* createEditor(QWidget* parent) = 0;

    virtual void loadConfig(QWidget* editor,
        const std::string& remoteContent,
        const std::string& localContent,
        const nlohmann::json& syncRules) = 0;

    virtual nlohmann::json saveSyncRules(QWidget* editor) const = 0;

    virtual std::string mergePreview(QWidget* editor) const = 0;

    // 追踪键在内容中的行列表 (1-based)，供 merge 预览标记。
    // 各格式插件自行实现键值对定位（含嵌套/段上下文），
    // 未支持时返回空数组（宿主回退整行标记）。
    virtual std::vector<int> trackedLines(
        const std::string& content,
        const std::vector<std::string>& trackedKeys) const
    {
        (void)content;
        (void)trackedKeys;
        return {};
    }
};

using CreateConfigEditorFunc = IConfigEditorExtension* (*)();

} // namespace NeoCore
