#pragma once

#include <QLibrary>
#include <QString>
#include <QStringList>
#include <QList>

#include <map>
#include <vector>
#include <memory>

#include <IConfigEditorExtension.h>
#include <IPointerEditorExtension.h>

namespace GUIWorker {

enum class EditorExtensionKind {
    Parser,
    Pointer
};

struct EditorExtensionInfo {
    QString name;
    QString version;
    QString description;
    QString dllName;
    EditorExtensionKind kind = EditorExtensionKind::Parser;
    QStringList fileTypes;
    // 扩展名冲突仲裁: 多个扩展声明同一后缀时, priority 高者注册 (默认 0)
    int priority = 0;
};

// 统一扩展注册表: 扫描 editor/extension 下所有 *.meta.json,
// 读取每个扩展的文件类型/功能类型并加载 DLL, 注册供查询与展示
class EditorExtensionRegistry {
public:
    EditorExtensionRegistry() = default;
    ~EditorExtensionRegistry();

    EditorExtensionRegistry(const EditorExtensionRegistry&) = delete;
    EditorExtensionRegistry& operator=(const EditorExtensionRegistry&) = delete;

    void scan(const QString& baseDir);
    void unloadAll();

    const QList<EditorExtensionInfo>& extensions() const;
    int count() const;

    NeoCore::IConfigEditorExtension* configEditorFor(const QString& fileExt) const;
    NeoCore::IPointerEditorExtension* pointerEditorFor(const QString& resolverType) const;

    QList<NeoCore::IPointerEditorExtension*> pointerEditors() const;
    QList<NeoCore::IConfigEditorExtension*> configEditors() const;

private:
    struct Entry {
        EditorExtensionInfo info;
        QLibrary* lib = nullptr;
        void* instance = nullptr;
    };

    void unloadEntry(Entry& e);

    QList<Entry> entries_;
    QList<EditorExtensionInfo> extensions_;
    std::map<QString, NeoCore::IConfigEditorExtension*> configMap_;
    std::map<QString, NeoCore::IPointerEditorExtension*> pointerMap_;
};

} // namespace GUIWorker
