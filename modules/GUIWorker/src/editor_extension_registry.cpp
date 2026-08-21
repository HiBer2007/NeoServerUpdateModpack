#include "editor_extension_registry.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>

#include <nlohmann/json.hpp>

#include <logger.h>

namespace GUIWorker {

const QList<EditorExtensionInfo>& EditorExtensionRegistry::extensions() const
{
    return extensions_;
}

int EditorExtensionRegistry::count() const
{
    return extensions_.size();
}

EditorExtensionRegistry::~EditorExtensionRegistry()
{
    unloadAll();
}

void EditorExtensionRegistry::unloadEntry(Entry& e)
{
    if (e.instance) {
        switch (e.info.kind) {
        case EditorExtensionKind::Parser:
            delete static_cast<NeoCore::IConfigEditorExtension*>(e.instance);
            break;
        case EditorExtensionKind::Pointer:
            delete static_cast<NeoCore::IPointerEditorExtension*>(e.instance);
            break;
        }
        e.instance = nullptr;
    }
    if (e.lib) {
        e.lib->unload();
        delete e.lib;
        e.lib = nullptr;
    }
}

void EditorExtensionRegistry::unloadAll()
{
    for (auto& e : entries_) {
        unloadEntry(e);
    }
    entries_.clear();
    extensions_.clear();
    configMap_.clear();
    pointerMap_.clear();
}

void EditorExtensionRegistry::scan(const QString& baseDir)
{
    QDir extRoot(baseDir);
    if (!extRoot.exists()) {
        CLogger::Info("EditorExtensionRegistry: dir not found: {}",
            baseDir.toStdString());
        return;
    }

    QDirIterator it(baseDir, { QStringLiteral("*.meta.json") }, QDir::Files,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString metaPath = it.next();
        QFile f(metaPath);
        if (!f.open(QIODevice::ReadOnly)) {
            CLogger::Warn("EditorExtensionRegistry: cannot read meta: {}",
                metaPath.toStdString());
            continue;
        }

        nlohmann::json meta;
        try {
            meta = nlohmann::json::parse(f.readAll().toStdString());
        } catch (const std::exception& e) {
            CLogger::Error("EditorExtensionRegistry: invalid meta {}: {}",
                metaPath.toStdString(), e.what());
            continue;
        }

        if (!meta.contains("dll")) {
            CLogger::Warn("EditorExtensionRegistry: meta without dll: {}",
                metaPath.toStdString());
            continue;
        }

        EditorExtensionInfo info;
        info.name = QString::fromStdString(meta.value("name", ""));
        info.version = QString::fromStdString(meta.value("version", ""));
        info.description = QString::fromStdString(meta.value("description", ""));
        info.dllName = QString::fromStdString(meta.value("dll", ""));
        const QString kindStr = QString::fromStdString(
            meta.value("editor_type", ""));

        if (kindStr == QLatin1String("parser")
            || meta.contains("extensions")) {
            info.kind = EditorExtensionKind::Parser;
            if (meta.contains("extensions") && meta["extensions"].is_array()) {
                for (const auto& e : meta["extensions"]) {
                    if (e.is_string()) {
                        info.fileTypes << QString::fromStdString(
                            e.get<std::string>());
                    }
                }
            }
        } else {
            info.kind = EditorExtensionKind::Pointer;
            if (meta.contains("resolver_type")) {
                info.fileTypes << QString::fromStdString(
                    meta.value("resolver_type", ""));
            }
        }

        const QString dllAbs = QFileInfo(metaPath).absoluteDir()
            .absoluteFilePath(info.dllName);
        if (!QFileInfo::exists(dllAbs)) {
            CLogger::Error("EditorExtensionRegistry: DLL missing for meta: {} "
                "(expected: {})", metaPath.toStdString(), dllAbs.toStdString());
            continue;
        }

        Entry entry;
        entry.info = info;
        entry.lib = new QLibrary(dllAbs);
        if (!entry.lib->load()) {
            CLogger::Error("EditorExtensionRegistry: load DLL failed: {}",
                dllAbs.toStdString());
            delete entry.lib;
            continue;
        }

        if (info.kind == EditorExtensionKind::Parser) {
            auto factory = reinterpret_cast<NeoCore::CreateConfigEditorFunc>(
                entry.lib->resolve("CreateConfigEditor"));
            if (!factory) {
                CLogger::Error("EditorExtensionRegistry: CreateConfigEditor not "
                    "found in {}", dllAbs.toStdString());
                entry.lib->unload();
                delete entry.lib;
                continue;
            }
            auto* inst = factory();
            if (!inst) {
                entry.lib->unload();
                delete entry.lib;
                continue;
            }
            if (info.fileTypes.isEmpty()) {
                info.fileTypes << QString::fromStdString(inst->fileExtension());
            }
            entry.info = info;
            entry.instance = inst;
            for (const QString& ext : info.fileTypes) {
                configMap_[ext.toLower()] = inst;
            }
        } else {
            auto factory = reinterpret_cast<NeoCore::CreateEditorExtensionFunc>(
                entry.lib->resolve("CreateEditorExtension"));
            if (!factory) {
                CLogger::Error("EditorExtensionRegistry: CreateEditorExtension "
                    "not found in {}", dllAbs.toStdString());
                entry.lib->unload();
                delete entry.lib;
                continue;
            }
            auto* inst = factory();
            if (!inst) {
                entry.lib->unload();
                delete entry.lib;
                continue;
            }
            if (info.fileTypes.isEmpty()) {
                info.fileTypes << QString::fromStdString(inst->resolverType());
            }
            entry.info = info;
            entry.instance = inst;
            for (const QString& type : info.fileTypes) {
                pointerMap_[type.toLower()] = inst;
            }
        }

        entries_.push_back(entry);
        extensions_.push_back(entry.info);
        CLogger::Info("EditorExtensionRegistry: registered {} [{}] (types: {})",
            entry.info.name.toStdString(),
            entry.info.kind == EditorExtensionKind::Parser ? "parser" : "pointer",
            entry.info.fileTypes.join(QLatin1Char(',')).toStdString());
    }

    CLogger::Info("EditorExtensionRegistry: loaded {} extension(s) from {}",
        entries_.size(), baseDir.toStdString());
}

NeoCore::IConfigEditorExtension* EditorExtensionRegistry::configEditorFor(
    const QString& fileExt) const
{
    auto it = configMap_.find(fileExt.toLower());
    return it != configMap_.end() ? it->second : nullptr;
}

NeoCore::IPointerEditorExtension* EditorExtensionRegistry::pointerEditorFor(
    const QString& resolverType) const
{
    auto it = pointerMap_.find(resolverType.toLower());
    return it != pointerMap_.end() ? it->second : nullptr;
}

QList<NeoCore::IPointerEditorExtension*> EditorExtensionRegistry::pointerEditors() const
{
    QList<NeoCore::IPointerEditorExtension*> list;
    list.reserve(pointerMap_.size());
    for (const auto& kv : pointerMap_) {
        if (!list.contains(kv.second)) {
            list.append(kv.second);
        }
    }
    return list;
}

QList<NeoCore::IConfigEditorExtension*> EditorExtensionRegistry::configEditors() const
{
    QList<NeoCore::IConfigEditorExtension*> list;
    list.reserve(configMap_.size());
    for (const auto& kv : configMap_) {
        if (!list.contains(kv.second)) {
            list.append(kv.second);
        }
    }
    return list;
}

} // namespace GUIWorker
