#include "modpack_content_ide.h"

#include "folder_policy_editor.h"
#include "config_file_editor.h"
#include "pointer_editor_panel.h"
#include "serverconfig_rules_editor.h"
#include "sync_policies_editor.h"
#include "file_content_editor.h"
#include "batch_editor_panel.h"
#include "editor_extension_registry.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QResizeEvent>
#include <QTimer>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileDialog>
#include <QPushButton>
#include <QPointer>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFrame>
#include <QApplication>
#include <QUndoStack>
#include <QUndoCommand>
#include <QSet>
#include <QCoreApplication>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include <map>
#include <set>
#include <functional>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <fstream>

#include <cancel_token.h>
#include <umd_generator.h>
#include <branch_merger.h>
#include <logger.h>
#include <IPluginPointer.h>

#include <crtdbg.h>

namespace GUIWorker {

namespace {

bool isTextEditableName(const QString& name)
{
    const QString lower = name.toLower();
    static const QStringList exts = {
        QStringLiteral("json"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("toml"), QStringLiteral("snbt"), QStringLiteral("txt"),
        QStringLiteral("properties"), QStringLiteral("cfg"), QStringLiteral("conf"),
        QStringLiteral("ini"), QStringLiteral("md"), QStringLiteral("log"),
        QStringLiteral("xml"), QStringLiteral("mcmeta"), QStringLiteral("lang"),
        QStringLiteral("css"), QStringLiteral("html"), QStringLiteral("htm"),
        QStringLiteral("js"), QStringLiteral("svg"),
    };
    for (const QString& e : exts) {
        if (lower.endsWith(QLatin1Char('.') + e)) return true;
    }
    return false;
}

struct FilePolicy {
    QString mode;
    QStringList keys;
    QVector<int> lines;
};

struct PolicySnapshot {
    QString defaultFolderPolicy;
    std::map<QString, QString> folders;
    std::map<QString, FilePolicy> files;
    std::set<QString> branchFolderOverrides;
    std::set<QString> branchFileOverrides;
};

void parsePolicyObject(const nlohmann::json& obj, PolicySnapshot& snap)
{
    if (obj.contains("default_folder_policy") && obj["default_folder_policy"].is_string()) {
        snap.defaultFolderPolicy =
            QString::fromStdString(obj["default_folder_policy"].get<std::string>());
    }
    if (obj.contains("folders") && obj["folders"].is_object()) {
        for (const auto& [path, val] : obj["folders"].items()) {
            if (!val.is_string()) continue;
            snap.folders[QString::fromStdString(path)] =
                QString::fromStdString(val.get<std::string>());
        }
    }
    if (obj.contains("files") && obj["files"].is_object()) {
        for (const auto& [path, val] : obj["files"].items()) {
            if (!val.is_object()) continue;
            FilePolicy fp;
            fp.mode = QString::fromStdString(val.value("mode", "full"));
            if (val.contains("tracked_keys") && val["tracked_keys"].is_array()) {
                for (const auto& k : val["tracked_keys"]) {
                    if (k.is_string()) {
                        fp.keys << QString::fromStdString(k.get<std::string>());
                    }
                }
            }
            if (val.contains("tracked_lines") && val["tracked_lines"].is_array()) {
                for (const auto& l : val["tracked_lines"]) {
                    if (l.is_number_integer()) {
                        fp.lines << l.get<int>();
                    }
                }
            }
            snap.files[QString::fromStdString(path)] = fp;
        }
    }
}

PolicySnapshot parsePolicies(const QString& repoDir, const QString& branch)
{
    PolicySnapshot snap;
    QFile f(repoDir + QStringLiteral("/workspace.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return snap;
    }
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();

        if (j.contains("sync_policies") && j["sync_policies"].is_object()) {
            parsePolicyObject(j["sync_policies"], snap);
        }

        if (j.contains("branches") && j["branches"].is_array()) {
            for (const auto& b : j["branches"]) {
                if (!b.is_object()) continue;
                if (b.value("name", "") != branch.toStdString()) continue;
                if (b.contains("sync_policies") && b["sync_policies"].is_object()) {
                    const auto& sp = b["sync_policies"];
                    PolicySnapshot br;
                    parsePolicyObject(sp, br);
                    if (!br.defaultFolderPolicy.isEmpty()) {
                        snap.defaultFolderPolicy = br.defaultFolderPolicy;
                    }
                    for (const auto& [path, policy] : br.folders) {
                        snap.folders[path] = policy;
                        snap.branchFolderOverrides.insert(path);
                    }
                    for (const auto& [path, fp] : br.files) {
                        snap.files[path] = fp;
                        snap.branchFileOverrides.insert(path);
                    }
                }
                break;
            }
        }
    } catch (...) {
        f.close();
    }
    return snap;
}

bool readPointerFile(const QString& branchConfigDir, const QString& sha,
    NeoCore::PointerFileData& out)
{
    QFile f(branchConfigDir + QLatin1Char('/') + sha
        + QStringLiteral(".pointer"));
    if (!f.open(QIODevice::ReadOnly)) {
        return false;
    }
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        out = NeoCore::PointerFileData::fromJson(j);
        return true;
    } catch (...) {
        f.close();
        return false;
    }
}

QString computeSha256(const QString& fp)
{
    QFile f(fp);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    f.close();
    return QString::fromLatin1(h.result().toHex());
}

bool updateFileManifest(const QString& bcDir, const QString& branch,
    const QString& relPath, const QString& sha)
{
    const QString filePath = bcDir + QLatin1Char('/') + branch
        + QStringLiteral(".json");
    nlohmann::json j;
    {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            try {
                j = nlohmann::json::parse(f.readAll().toStdString());
            } catch (...) {}
            f.close();
        }
    }
    if (!j.is_object()) {
        j["file_manifest"] = nlohmann::json::object();
        j["pointer_files"] = nlohmann::json::object();
    }
    if (!j.contains("file_manifest") || !j["file_manifest"].is_object()) {
        j["file_manifest"] = nlohmann::json::object();
    }
    j["file_manifest"][relPath.toStdString()] = sha.toStdString();
    std::ofstream f(filePath.toStdString());
    if (!f.is_open()) return false;
    f << j.dump(2) << std::endl;
    f.close();
    return true;
}

void removeFromFileManifest(const QString& bcDir, const QString& branch,
    const QString& relPath)
{
    const QString filePath = bcDir + QLatin1Char('/') + branch
        + QStringLiteral(".json");
    nlohmann::json j;
    {
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly)) {
            try {
                j = nlohmann::json::parse(f.readAll().toStdString());
            } catch (...) {}
            f.close();
        }
    }
    if (!j.is_object()) return;
    if (j.contains("file_manifest") && j["file_manifest"].is_object()) {
        j["file_manifest"].erase(relPath.toStdString());
        std::ofstream f(filePath.toStdString());
        if (f.is_open()) {
            f << j.dump(2) << std::endl;
            f.close();
        }
    }
}

QMap<QString, QString> readBranchManifest(const QString& branchDir)
{
    QMap<QString, QString> markers;
    QFile f(branchDir + QStringLiteral("/branch_manifest.json"));
    if (!f.open(QIODevice::ReadOnly)) return markers;
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        if (j.contains("markers") && j["markers"].is_object()) {
            for (const auto& [rel, val] : j["markers"].items()) {
                if (val.is_string()) {
                    markers[QString::fromStdString(rel)] =
                        QString::fromStdString(val.get<std::string>());
                }
            }
        }
    } catch (...) {
        f.close();
    }
    return markers;
}

// 读 branch_config/<branch>.json: file_manifest (rel → sha, 兼容 string 与 {sha256})
// + pointer_files (sha → {resolver, metadata}) → rel → sha / rel → resolver 映射。
// 仅当 sha 存在于 pointer_files (有 resolver, 即真实指针配置) 才计入指针,
// 普通文件 (file_manifest 无指针配置) 不纳入。
void collectPointerMaps(const QString& bcDir, const QString& branch,
    QMap<QString, QString>& relToSha, QMap<QString, QString>& relToResolver)
{
    relToSha.clear();
    relToResolver.clear();
    QFile f(bcDir + QLatin1Char('/') + branch + QStringLiteral(".json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();

        std::unordered_map<std::string, std::string> shaToResolver;
        if (j.contains("pointer_files") && j["pointer_files"].is_object()) {
            for (const auto& [sha, val] : j["pointer_files"].items()) {
                if (val.is_object()) {
                    const std::string resolver = val.value("resolver", "");
                    if (!resolver.empty()) {
                        shaToResolver[sha] = resolver;
                    }
                }
            }
        }
        if (j.contains("file_manifest") && j["file_manifest"].is_object()) {
            for (const auto& [rel, val] : j["file_manifest"].items()) {
                std::string sha;
                if (val.is_string()) {
                    sha = val.get<std::string>();
                } else if (val.is_object()) {
                    sha = val.value("sha256", "");
                }
                if (sha.empty()) continue;
                const auto it = shaToResolver.find(sha);
                if (it == shaToResolver.end()) continue;
                relToSha[QString::fromStdString(rel)] =
                    QString::fromStdString(sha);
                relToResolver[QString::fromStdString(rel)] =
                    QString::fromStdString(it->second);
            }
        }
    } catch (...) {
        f.close();
    }
}

// 按 rel 反查指针 sha (输出树指针项点击用)
QString pointerShaForRel(const QString& bcDir, const QString& branch,
    const QString& rel)
{
    QMap<QString, QString> relToSha;
    QMap<QString, QString> relToResolver;
    collectPointerMaps(bcDir, branch, relToSha, relToResolver);
    return relToSha.value(rel);
}

void writeBranchManifest(const QString& branchDir,
    const QMap<QString, QString>& markers)
{
    QDir().mkpath(branchDir);
    nlohmann::json m = nlohmann::json::object();
    for (auto it = markers.begin(); it != markers.end(); ++it) {
        if (it.value() == QLatin1String("delete")
            || it.value() == QLatin1String("override")) {
            m[it.key().toStdString()] = it.value().toStdString();
        }
    }
    nlohmann::json j;
    j["markers"] = m;
    std::ofstream f((branchDir + QStringLiteral("/branch_manifest.json"))
        .toStdString());
    if (f.is_open()) {
        f << j.dump(2) << std::endl;
        f.close();
    }
}

QStringList buildBranchChain(const QString& repoDir, const QString& branch)
{
    QStringList chain;
    QMap<QString, QString> parents;
    {
        QFile f(repoDir + QStringLiteral("/workspace.json"));
        if (f.open(QIODevice::ReadOnly)) {
            try {
                const auto j = nlohmann::json::parse(f.readAll().toStdString());
                f.close();
                if (j.contains("branches") && j["branches"].is_array()) {
                    for (const auto& b : j["branches"]) {
                        if (!b.is_object()) continue;
                        const QString name =
                            QString::fromStdString(b.value("name", ""));
                        if (name.isEmpty()) continue;
                        const QString parent = b.contains("parent")
                            && b["parent"].is_string()
                            ? QString::fromStdString(b["parent"].get<std::string>())
                            : QString();
                        parents[name] = parent;
                    }
                }
            } catch (...) {
                f.close();
            }
        }
    }
    QSet<QString> visited;
    QString cur = branch;
    while (!cur.isEmpty() && !visited.contains(cur)) {
        visited.insert(cur);
        chain.prepend(cur);
        cur = parents.value(cur);
    }
    return chain;
}

bool fileExistsInParentChain(const QString& repoDir,
    const QStringList& chain, const QString& rel)
{
    for (int i = chain.size() - 2; i >= 0; --i) {
        const QString dir = repoDir + QStringLiteral("/branches/")
            + chain[i];
        const auto markers = readBranchManifest(dir);
        if (markers.value(rel) == QLatin1String("delete")) return false;
        if (QFileInfo::exists(dir + QLatin1Char('/') + rel)) return true;
        if (QFileInfo::exists(dir + QStringLiteral("/.overrides/") + rel)) {
            return true;
        }
    }
    return false;
}

QStringList collectInheritedFiles(const QString& repoDir,
    const QStringList& chain, const QString& selfDir)
{
    QStringList result;
    if (chain.size() < 2) return result;

    const auto selfMarkers = readBranchManifest(selfDir);
    QSet<QString> selfFiles;
    {
        QDirIterator it(selfDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = QDir(selfDir).relativeFilePath(it.filePath());
            if (rel.startsWith(QStringLiteral(".overrides/"))
                || rel == QStringLiteral("branch_manifest.json")) continue;
            selfFiles.insert(rel);
        }
    }

    QSet<QString> collected;
    for (int i = chain.size() - 2; i >= 0; --i) {
        const QString dir = repoDir + QStringLiteral("/branches/")
            + chain[i];
        if (!QDir(dir).exists()) continue;
        const auto markers = readBranchManifest(dir);

        QStringList files;
        QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel = QDir(dir).relativeFilePath(it.filePath());
            if (rel.startsWith(QStringLiteral(".overrides/"))
                || rel == QStringLiteral("branch_manifest.json")) continue;
            files << rel;
        }
        const QString ovDir = dir + QStringLiteral("/.overrides");
        QDirIterator it3(ovDir, QDir::Files, QDirIterator::Subdirectories);
        while (it3.hasNext()) {
            it3.next();
            files << QDir(ovDir).relativeFilePath(it3.filePath());
        }

        for (const QString& rel : files) {
            if (collected.contains(rel)) continue;
            collected.insert(rel);
            if (selfFiles.contains(rel)) continue;
            if (selfMarkers.contains(rel)) continue;
            if (markers.value(rel) == QLatin1String("delete")) continue;
            result << rel;
        }
    }
    return result;
}

// 异步撤销/重做的进度回调: (percent, 状态文本)
using ProgressCb = std::function<void(int, const QString&)>;

class BranchFileCommand : public QUndoCommand {
public:
    BranchFileCommand(std::function<void()> doFn, std::function<void()> undoFn,
        const QString& text)
        : QUndoCommand(text)
        , doFn_(std::move(doFn))
        , undoFn_(std::move(undoFn))
    {
    }

    // 异步模式: 耗时文件操作在后台线程执行 (进度回调), 命令 text 作为卡片标题
    BranchFileCommand(std::function<void()> doFn, std::function<void()> undoFn,
        const QString& text, ModpackContentIde* host,
        std::function<void(const ProgressCb&)> asyncDo,
        std::function<void(const ProgressCb&)> asyncUndo,
        std::function<void()> asyncFinish = {})
        : QUndoCommand(text)
        , doFn_(std::move(doFn))
        , undoFn_(std::move(undoFn))
        , host_(host)
        , asyncDo_(std::move(asyncDo))
        , asyncUndo_(std::move(asyncUndo))
        , asyncFinish_(std::move(asyncFinish))
    {
    }

    void redo() override
    {
        if (host_ && asyncDo_) {
            host_->runAsyncCommand(text(), asyncDo_, asyncFinish_);
            return;
        }
        if (doFn_) doFn_();
    }

    void undo() override
    {
        if (host_ && asyncUndo_) {
            host_->runAsyncCommand(text(), asyncUndo_, asyncFinish_);
            return;
        }
        if (undoFn_) undoFn_();
    }

private:
    std::function<void()> doFn_;
    std::function<void()> undoFn_;
    ModpackContentIde* host_ = nullptr;
    std::function<void(const ProgressCb&)> asyncDo_;
    std::function<void(const ProgressCb&)> asyncUndo_;
    std::function<void()> asyncFinish_;
};

struct DeleteState {
    QStringList parentHasRels;
    QMap<QString, QString> movedSha;
};

struct OverwriteState {
    QString rel;
    QString trashRel;
    QString shaBefore;
    bool parentHas = false;
    bool movedOld = false;
    bool wroteMarker = false;
};

struct RestoreState {
    QString rel;
    QString marker;
    QString trashRel;
    QString shaBefore;
    bool movedEntity = false;
};

QVector<ImportJob> makeImportJobs(const QStringList& filePaths,
    const QString& targetRel)
{
    QVector<ImportJob> jobs;
    auto appendFile = [&jobs](const QString& src, const QString& relDir) {
        const QString name = QFileInfo(src).fileName();
        QString rel;
        if (relDir.isEmpty()) {
            rel = name;
        } else {
            rel = relDir + QLatin1Char('/') + name;
        }
        jobs.push_back({ src, rel });
    };
    for (const QString& p : filePaths) {
        if (QFileInfo(p).isDir()) {
            const QString folderName = QFileInfo(p).fileName();
            QDirIterator it(p, QDir::Files | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                const QString sub = QDir(p).relativeFilePath(f);
                QString relDir = targetRel;
                if (!relDir.isEmpty()) {
                    relDir += QLatin1Char('/');
                }
                relDir += folderName;
                const QString subPath = QFileInfo(sub).path();
                if (!subPath.isEmpty() && subPath != QStringLiteral(".")) {
                    relDir += QLatin1Char('/') + subPath;
                }
                appendFile(f, relDir);
            }
        } else {
            appendFile(p, targetRel);
        }
    }
    return jobs;
}

} // namespace

ModpackContentIde::ModpackContentIde(QWidget* parent)
    : QWidget(parent)
    , previewRunning_(false)
{
    undoStack_ = new QUndoStack(this);
    extRegistry_ = std::make_unique<EditorExtensionRegistry>();
    scanExtensionDirs(extRegistry_.get());
    buildUI();

    // 周期刷新仓库文件树 (拖入/外部修改后磁盘变化自动可见)
    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(3000);
    connect(refreshTimer_, &QTimer::timeout, this, &ModpackContentIde::refreshFileTree);
    refreshTimer_->start();
}

ModpackContentIde::~ModpackContentIde()
{
    // 先于成员 extRegistry_ 析构 (unload 扩展 DLL) 释放扩展编辑器控件:
    // 否则析构链中 delete DLL 创建的对象时其 vtable 已指向被卸载的代码页
    // → ACCESS_VIOLATION (2026-08-20 崩溃: clearResolverEditors 内 Qt6Widgetsd 崩溃)
    if (pointerEditor_) {
        pointerEditor_->setExtensionRegistry(nullptr);
    }
    if (cancelToken_) {
        cancelToken_->request_cancel();
    }
    if (previewThread_ && previewThread_->joinable()) {
        previewThread_->join();
    }
    if (importCancelToken_) {
        importCancelToken_->request_cancel();
    }
    if (importThread_ && importThread_->joinable()) {
        importThread_->join();
    }
    // 异步撤销/重做工作线程与预览/导入线程: 析构前等待完成 (闭包捕获 this)
    for (auto& t : workThreads_) {
        if (t && t->joinable()) {
            t->join();
        }
    }
    workThreads_.clear();
}

void ModpackContentIde::scanExtensionDirs(EditorExtensionRegistry* reg)
{
    QStringList dirs;
    dirs << QCoreApplication::applicationDirPath()
        + QStringLiteral("/editor/extension");
    dirs << QCoreApplication::applicationDirPath()
        + QStringLiteral("/../editor/extension");
    dirs << QDir::currentPath()
        + QStringLiteral("/build/deploy/editor/extension");

    // 去重 (cwd 与 exe 目录相同时 /../ 与 / 同源), 避免同一 DLL 二次加载
    QSet<QString> seen;
    for (const QString& raw : dirs) {
        const QString canon = QDir(raw).absolutePath();
        if (seen.contains(canon)) {
            continue;
        }
        seen.insert(canon);
        reg->scan(canon);
    }
}

void ModpackContentIde::rescanExtensions()
{
    auto old = std::move(extRegistry_);
    auto reg = std::make_unique<EditorExtensionRegistry>();
    scanExtensionDirs(reg.get());
    extRegistry_ = std::move(reg);
    if (pointerEditor_) {
        pointerEditor_->setExtensionRegistry(extRegistry_.get());
    }
    emit extensionRegistryChanged();
    CLogger::Info("ModpackContentIde: extension rescan done, {} registered",
        extRegistry_->count());
}

void ModpackContentIde::buildUI()
{
    auto* leftTabs = new QTabWidget(this);
    leftTabs->setDocumentMode(true);
    leftTabs->setTabPosition(QTabWidget::North);
    leftTabs_ = leftTabs;

    outputPanel_ = new OutputTreePanel(leftTabs);
    CLogger::Info("BISECT heap after OutputTreePanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    repoPanel_ = new RepoTreePanel(leftTabs);
    CLogger::Info("BISECT heap after RepoTreePanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    optionsPanel_ = new QWidget(leftTabs);

    leftTabs->addTab(outputPanel_, QString::fromUtf8("\u8f93\u51fa\u6587\u4ef6\u6811"));
    leftTabs->addTab(repoPanel_, QString::fromUtf8("\u4ed3\u5e93\u6587\u4ef6\u6811"));
    leftTabs->addTab(optionsPanel_, QString::fromUtf8("\u9879\u76ee\u9009\u9879"));

    editorStack_ = new QStackedWidget(this);
    auto* emptyEditor = new QWidget(editorStack_);
    auto* emptyLay = new QVBoxLayout(emptyEditor);
    auto* emptyHint = new QLabel(
        QString::fromUtf8("\u9009\u62e9\u5de6\u4fa7\u5bf9\u8c61\u5f00\u59cb\u7f16\u8f91\u2026"),
        emptyEditor);
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 13px;"));
    emptyLay->addWidget(emptyHint);
    editorStack_->addWidget(emptyEditor);

    folderEditor_ = new FolderPolicyEditor(editorStack_);
    editorStack_->addWidget(folderEditor_);
    CLogger::Info("BISECT heap after FolderPolicyEditor: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    configEditor_ = new ConfigFileEditor(editorStack_);
    editorStack_->addWidget(configEditor_);
    CLogger::Info("BISECT heap after ConfigFileEditor: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    pointerEditor_ = new PointerEditorPanel(editorStack_);
    pointerEditor_->setExtensionRegistry(extRegistry_.get());
    editorStack_->addWidget(pointerEditor_);
    CLogger::Info("BISECT heap after PointerEditorPanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    serverConfigEditor_ = new ServerConfigRulesEditor(editorStack_);
    editorStack_->addWidget(serverConfigEditor_);
    CLogger::Info("BISECT heap after ServerConfigRulesEditor: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    auto* contentEditor = new FileContentEditor(editorStack_);
    contentEditor->setObjectName(QStringLiteral("fileContentEditor"));
    editorStack_->addWidget(contentEditor);
    connect(contentEditor, &FileContentEditor::contentSaveRequested, this,
        &ModpackContentIde::onContentSave);

    batchEditor_ = new BatchEditorPanel(editorStack_);
    editorStack_->addWidget(batchEditor_);
    CLogger::Info("BISECT heap after BatchEditorPanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    // 注意: editorStack_ 不再挂 QGraphicsOpacityEffect 淡入动画——
    // QGraphicsEffect 把整个子树渲染到离屏缓冲再合成, 动态内容
    // (解析器卡片/QScrollArea) 缓存失效不及时 → 内容不刷新/需窗口重绘

    auto* mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->addWidget(leftTabs);
    mainSplitter->addWidget(editorStack_);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 3);
    mainSplitter->setSizes({280, 680});

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(mainSplitter);

    // 右上角层叠工作卡片堆 (紧凑, 自锚定右上角) + 错误 toast (位于卡片堆上方)
    workStack_ = new HiBerGUI::WorkCardStack(this);
    toast_ = new HiBerGUI::ToastNotification(this);

    buildOptionsPanel();
    CLogger::Info("BISECT heap after buildOptionsPanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    connect(outputPanel_, &OutputTreePanel::formatChanged, this,
        [this](const QString&) { refreshPreview(); });
    connect(outputPanel_, &OutputTreePanel::refreshRequested, this,
        &ModpackContentIde::refreshPreview);
    connect(outputPanel_, &OutputTreePanel::filesDropped, this,
        &ModpackContentIde::importDroppedFiles);
    // 拖放悬停: 状态栏指示落点文件夹
    connect(outputPanel_, &OutputTreePanel::dropTargetChanged, this,
        [this](const QString& rel, bool hovering) {
            showDropTargetHint(rel, hovering);
        });
    connect(outputPanel_, &OutputTreePanel::deleteRequested, this,
        [this](const RepoObjectInfo& info) {
            deletePath(info.path, info.type == RepoObjectType::Folder);
        });
    connect(outputPanel_, &OutputTreePanel::batchDeleteRequested, this,
        [this](const QList<RepoObjectInfo>& infos) {
            deletePaths(infos);
        });
    connect(outputPanel_, &OutputTreePanel::pasteRequested, this,
        [this](const QStringList& relPaths, const QString& targetRel,
            bool isCut) {
            pasteBranchFiles(relPaths, targetRel, isCut);
        });
    connect(outputPanel_, &OutputTreePanel::objectActivated, this,
        [this](const RepoObjectInfo& info) {
            onTreeObjectActivated(outputPanel_, info);
        });
    connect(outputPanel_, &OutputTreePanel::newFolderRequested, this,
        &ModpackContentIde::createBranchFolder);
    connect(outputPanel_, &OutputTreePanel::createServerConfigRequested, this,
        &ModpackContentIde::createServerConfigFolder);
    connect(repoPanel_, &RepoTreePanel::objectActivated, this,
        [this](const RepoObjectInfo& info) {
            onTreeObjectActivated(repoPanel_, info);
        });
    connect(repoPanel_, &RepoTreePanel::createServerConfigRequested, this,
        &ModpackContentIde::createServerConfigFolder);
    connect(repoPanel_, &RepoTreePanel::filesDropped, this,
        &ModpackContentIde::importDroppedFiles);
    connect(repoPanel_, &RepoTreePanel::dropTargetChanged, this,
        [this](const QString& rel, bool hovering) {
            showDropTargetHint(rel, hovering);
        });
    connect(repoPanel_, &RepoTreePanel::deleteRequested, this,
        [this](const RepoObjectInfo& info) {
            deletePath(info.path, info.type == RepoObjectType::Folder);
        });
    connect(repoPanel_, &RepoTreePanel::batchDeleteRequested, this,
        [this](const QList<RepoObjectInfo>& infos) {
            deletePaths(infos);
        });
    connect(repoPanel_, &RepoTreePanel::pasteRequested, this,
        [this](const QStringList& relPaths, const QString& targetRel,
            bool isCut) {
            pasteBranchFiles(relPaths, targetRel, isCut);
        });
    connect(repoPanel_, &RepoTreePanel::batchRestorePointersRequested, this,
        [this](const QList<RepoObjectInfo>& infos) {
            batchRestorePointers(infos);
        });
    connect(repoPanel_, &RepoTreePanel::moveItemsRequested, this,
        [this](const QList<RepoObjectInfo>& infos, const QString& targetRel,
            bool copy) {
            pasteBranchFiles(infosToPaths(infos), targetRel, !copy);
        });
    connect(repoPanel_, &RepoTreePanel::restoreInheritedRequested, this,
        [this](const RepoObjectInfo& info) {
            restoreInherited(info.path);
        });
    connect(repoPanel_, &RepoTreePanel::contentEditRequested, this,
        [this](const RepoObjectInfo& info) {
            openContentEditor(info);
        });
    connect(repoPanel_, &RepoTreePanel::folderPolicyEditRequested, this,
        [this](const RepoObjectInfo& info) {
            routeObject(info);
        });
    // 普通文件 ↔ 配置文件标记
    connect(repoPanel_, &RepoTreePanel::markAsConfigFileRequested, this,
        [this](const RepoObjectInfo& info) {
            emit configFileMarkChanged(info.path, true);
        });
    connect(repoPanel_, &RepoTreePanel::unmarkConfigFileRequested, this,
        [this](const RepoObjectInfo& info) {
            emit configFileMarkChanged(info.path, false);
        });
    connect(outputPanel_, &OutputTreePanel::markAsConfigFileRequested, this,
        [this](const RepoObjectInfo& info) {
            emit configFileMarkChanged(info.path, true);
        });
    connect(outputPanel_, &OutputTreePanel::unmarkConfigFileRequested, this,
        [this](const RepoObjectInfo& info) {
            emit configFileMarkChanged(info.path, false);
        });
    connect(repoPanel_, &RepoTreePanel::filePolicyEditRequested, this,
        [this](const RepoObjectInfo& info) {
            const PolicySnapshot snap = parsePolicies(repoDir_, branch_);
            const auto it = snap.files.find(info.path);
            const bool overrides = snap.branchFileOverrides.count(info.path) > 0;
            FilePolicy fp;
            if (it != snap.files.end()) {
                fp = it->second;
            }
            const QString absPath = repoDir_ + QStringLiteral("/branches/")
                + branch_ + QLatin1Char('/') + info.path;
            std::vector<std::string> keys;
            for (const auto& k : fp.keys) {
                keys.push_back(k.toStdString());
            }
            std::vector<int> lines;
            for (int l : fp.lines) {
                lines.push_back(l);
            }
            injectConfigEditorExt(info.path);
            configEditor_->load(info.path, absPath, repoDir_, branch_, fp.mode,
                keys, lines, overrides);
            switchEditor(2);
        });
    connect(repoPanel_, &RepoTreePanel::importOverwriteRequested, this,
        [this](const RepoObjectInfo& info) {
            const QString path = QFileDialog::getOpenFileName(this,
                QString::fromUtf8("\u9009\u62e9\u8986\u76d6\u6587\u4ef6"),
                QDir::homePath());
            if (path.isEmpty()) return;
            const QString targetRel = QFileInfo(info.path).path();
            startImport({ path }, targetRel, false, true);
        });
    connect(folderEditor_, &FolderPolicyEditor::saveRequested, this,
        [this](const QString& path, const QString& policy, bool toBranch) {
            emit folderPolicySaveRequested(path, policy, toBranch);
            // 保存后重新加载当前文件夹编辑器, 状态文字动态刷新为磁盘现状
            refreshFolderEditorState();
        });
    connect(folderEditor_, &FolderPolicyEditor::applyToSubfoldersRequested, this,
        &ModpackContentIde::applyFolderPolicyToSubfolders);
    connect(folderEditor_, &FolderPolicyEditor::contentModified, this,
        &ModpackContentIde::contentModified);
    connect(configEditor_, &ConfigFileEditor::saveRequested, this,
        &ModpackContentIde::filePolicySaveRequested);
    connect(configEditor_, &ConfigFileEditor::contentModified, this,
        &ModpackContentIde::contentModified);
    connect(pointerEditor_, &PointerEditorPanel::pointerSaved, this,
        [this](const QString&) { emit contentModified(); });
    connect(pointerEditor_, &PointerEditorPanel::branchConfigChanged, this,
        &ModpackContentIde::branchConfigChanged);
    connect(pointerEditor_, &PointerEditorPanel::gitAddRequested, this,
        &ModpackContentIde::gitAddRequested);
    connect(pointerEditor_, &PointerEditorPanel::logMessage, this,
        &ModpackContentIde::logMessage);
    connect(pointerEditor_, &PointerEditorPanel::requestRefresh, this,
        &ModpackContentIde::refreshPreview);
    connect(serverConfigEditor_, &ServerConfigRulesEditor::gitAddRequested, this,
        &ModpackContentIde::gitAddRequested);
    connect(serverConfigEditor_, &ServerConfigRulesEditor::logMessage, this,
        &ModpackContentIde::logMessage);
    connect(batchEditor_, &BatchEditorPanel::batchPolicySaveRequested, this,
        [this](const QStringList& paths, const QString& mode,
            const std::vector<std::string>& keys,
            const std::vector<int>& lines, bool toBranch) {
            applyBatchPolicy(paths, mode, keys, lines, toBranch);
        });
    connect(batchEditor_, &BatchEditorPanel::batchConvertJarsRequested, this,
        [this](const QStringList& relPaths) {
            if (repoDir_.isEmpty() || branch_.isEmpty() || relPaths.isEmpty()) {
                return;
            }
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->batchConvertJarsList(relPaths);
            switchEditor(3);
        });
    connect(batchEditor_, &BatchEditorPanel::batchRestorePointersRequested, this,
        [this](const QList<RepoObjectInfo>& infos) {
            batchRestorePointers(infos);
        });
    connect(batchEditor_, &BatchEditorPanel::batchDeleteRequested, this,
        [this](const QList<RepoObjectInfo>& infos) {
            deletePaths(infos);
        });
    connect(batchEditor_, &BatchEditorPanel::contentModified, this,
        &ModpackContentIde::contentModified);
    connect(repoPanel_, &RepoTreePanel::batchConvertJarsRequested, this,
        [this](const QString& folderPath) {
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->batchConvertJars(folderPath);
            switchEditor(3);
        });
    connect(pointerEditor_, &PointerEditorPanel::batchConvertFinished, this,
        [this](const QVector<ConvertedItem>& items, int failed) {
            Q_UNUSED(failed);
            if (items.isEmpty()) return;
            auto converted = std::make_shared<QVector<ConvertedItem>>(items);
            auto asyncDo = [this, converted](const ProgressCb& cb) {
                cb(10, QString::fromUtf8("\u91cd\u505a\u8f6c\u6362\u2026"));
                pointerEditor_->redoBatchConvert(*converted);
                cb(100, QString::fromUtf8("\u5b8c\u6210"));
            };
            auto asyncUndo = [this, converted](const ProgressCb& cb) {
                cb(10, QString::fromUtf8("\u64a4\u9500\u8f6c\u6362\u2026"));
                pointerEditor_->undoBatchConvert(*converted);
                cb(100, QString::fromUtf8("\u5b8c\u6210"));
            };
            undoStack_->push(new BranchFileCommand(
                [this, converted]() {
                    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
                    pointerEditor_->redoBatchConvert(*converted);
                },
                [this, converted]() {
                    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
                    pointerEditor_->undoBatchConvert(*converted);
                },
                QString::fromUtf8("\u6279\u91cf\u8f6c\u6362 JAR\u2192\u6307\u9488"),
                this, std::move(asyncDo), std::move(asyncUndo)));
        });
    connect(repoPanel_, &RepoTreePanel::convertToPointerRequested, this,
        [this](const RepoObjectInfo& info) {
            const QString absPath = repoDir_ + QStringLiteral("/branches/")
                + branch_ + QLatin1Char('/') + info.path;
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->loadFileToConvert(info.path, absPath);
            switchEditor(3);
        });
    connect(repoPanel_, &RepoTreePanel::restorePointerRequested, this,
        [this](const QString& sha) {
            NeoCore::PointerFileData pfd;
            if (!readPointerFile(branchConfigDir_, sha, pfd)) return;
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->loadPointer(sha, pfd);
            switchEditor(3);
        });
    // 卡片取消由各卡片自身连接 (导入/预览分别取消对应 token)
}

void ModpackContentIde::buildOptionsPanel()
{
    auto* lay = new QVBoxLayout(optionsPanel_);
    lay->setContentsMargins(8, 6, 8, 8);
    lay->setSpacing(4);

    auto makeHLine = [optionsPanel_ = optionsPanel_]() {
        auto* line = new QFrame(optionsPanel_);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(QStringLiteral("color: #3a4048;"));
        return line;
    };

    auto* title = new QLabel(QString::fromUtf8("\u9879\u76ee\u9009\u9879"), optionsPanel_);
    title->setStyleSheet(QStringLiteral("font-weight: bold; color: #e8eaed;"));
    lay->addWidget(title);

    lay->addWidget(makeHLine());

    auto* policiesLabel = new QLabel(QString::fromUtf8("\u9876\u5c42\u540c\u6b65\u7b56\u7565 (sync_policies)"),
        optionsPanel_);
    policiesLabel->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    lay->addWidget(policiesLabel);

    auto* editor = new SyncPoliciesEditor(optionsPanel_);
    editor->setObjectName(QStringLiteral("topSyncEditor"));
    lay->addWidget(editor);

    // 项目选项修改: 触发已修改标记 + 防抖自动保存 (800ms 无新修改后保存到 workspace.json)
    connect(editor, &SyncPoliciesEditor::contentModified, this, [this]() {
        emit contentModified();
        auto* ed = optionsPanel_->findChild<SyncPoliciesEditor*>(
            QStringLiteral("topSyncEditor"));
        if (!ed || repoDir_.isEmpty()) return;
        const int myGen = ++topSyncSaveGen_;
        QTimer::singleShot(800, this, [this, myGen]() {
            if (myGen != topSyncSaveGen_) return;
            auto* e = optionsPanel_->findChild<SyncPoliciesEditor*>(
                QStringLiteral("topSyncEditor"));
            if (!e || repoDir_.isEmpty()) return;
            const nlohmann::json sp = e->save();
            emit topSyncPoliciesSaveRequested(sp.is_null()
                ? QStringLiteral("null")
                : QString::fromStdString(sp.dump()));
        });
    });

    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(6);
    auto* topSaveBtn = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u9876\u5c42\u540c\u6b65\u7b56\u7565"),
        optionsPanel_);
    auto* topBtn = new QPushButton(QString::fromUtf8("\u7f16\u8f91\u9876\u5c42\u6587\u4ef6\u5939\u7b56\u7565..."),
        optionsPanel_);
    auto* topFileBtn = new QPushButton(QString::fromUtf8("\u7f16\u8f91\u9876\u5c42\u6587\u4ef6\u7b56\u7565..."),
        optionsPanel_);
    topRow->addWidget(topSaveBtn);
    topRow->addWidget(topBtn);
    topRow->addWidget(topFileBtn);
    topRow->addStretch(1);
    lay->addLayout(topRow);

    connect(topSaveBtn, &QPushButton::clicked, this, [this]() {
        if (repoDir_.isEmpty()) {
            emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u3002"));
            return;
        }
        auto* editor = optionsPanel_->findChild<SyncPoliciesEditor*>(
            QStringLiteral("topSyncEditor"));
        if (!editor) return;
        const nlohmann::json sp = editor->save();
        emit topSyncPoliciesSaveRequested(sp.is_null()
            ? QStringLiteral("null")
            : QString::fromStdString(sp.dump()));
    });

    lay->addWidget(makeHLine());

    auto* modLabel = new QLabel(QString::fromUtf8("\u81ea\u5b9a\u4e49\u6a21\u7ec4\u8bbe\u7f6e"),
        optionsPanel_);
    modLabel->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    lay->addWidget(modLabel);

    auto* modRow = new QHBoxLayout();
    modRow->setSpacing(6);
    auto* modPolicyBtn = new QPushButton(QString::fromUtf8("\u7f16\u8f91 mods \u6587\u4ef6\u5939\u7b56\u7565..."),
        optionsPanel_);
    auto* modConvertBtn = new QPushButton(
        QString::fromUtf8("\u6279\u91cf\u8f6c\u6362 JAR\u2192Modrinth \u6307\u9488..."), optionsPanel_);
    modRow->addWidget(modPolicyBtn);
    modRow->addWidget(modConvertBtn);
    modRow->addStretch(1);
    lay->addLayout(modRow);

    connect(topBtn, &QPushButton::clicked, this, [this]() {
        if (repoDir_.isEmpty() || branch_.isEmpty()) {
            emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
            return;
        }
        bool ok = false;
        const QString path = QInputDialog::getText(this,
            QString::fromUtf8("\u7f16\u8f91\u9876\u5c42\u6587\u4ef6\u5939\u7b56\u7565"),
            QString::fromUtf8("\u6587\u4ef6\u5939\u76f8\u5bf9\u8def\u5f84 (e.g. mods):"),
            QLineEdit::Normal, QStringLiteral("mods"), &ok);
        if (!ok || path.trimmed().isEmpty()) return;

        const auto snap = parsePolicies(repoDir_, branch_);
        QString eff = snap.defaultFolderPolicy;
        const auto it = snap.folders.find(path.trimmed());
        if (it != snap.folders.end()) {
            eff = it->second;
        }
        folderEditor_->load(path.trimmed(), eff, false, QString());
        folderEditor_->setScopeTop();
        switchEditor(1);
    });

    connect(topFileBtn, &QPushButton::clicked, this, [this]() {
        if (repoDir_.isEmpty() || branch_.isEmpty()) {
            emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
            return;
        }
        bool ok = false;
        const QString path = QInputDialog::getText(this,
            QString::fromUtf8("\u7f16\u8f91\u9876\u5c42\u6587\u4ef6\u7b56\u7565"),
            QString::fromUtf8("\u6587\u4ef6\u76f8\u5bf9\u8def\u5f84 (e.g. config/example.toml):"),
            QLineEdit::Normal, QStringLiteral("config/example.toml"), &ok);
        if (!ok || path.trimmed().isEmpty()) return;

        const auto snap = parsePolicies(repoDir_, branch_);
        const auto it = snap.files.find(path.trimmed());
        injectConfigEditorExt(path.trimmed());
        if (it == snap.files.end()) {
            configEditor_->load(path.trimmed(), QString(), repoDir_, branch_,
                QString(), std::vector<std::string>(), std::vector<int>(), false);
        } else {
            std::vector<std::string> keys;
            keys.reserve(it->second.keys.size());
            for (const QString& k : it->second.keys) {
                keys.push_back(k.toStdString());
            }
            std::vector<int> lines;
            lines.reserve(it->second.lines.size());
            for (int l : it->second.lines) {
                lines.push_back(l);
            }
            configEditor_->load(path.trimmed(), QString(), repoDir_, branch_,
                it->second.mode, keys, lines, false);
        }
        configEditor_->setScopeTop();
        switchEditor(2);
    });

    connect(modPolicyBtn, &QPushButton::clicked, this, [this]() {
        if (repoDir_.isEmpty() || branch_.isEmpty()) {
            emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
            return;
        }
        const auto snap = parsePolicies(repoDir_, branch_);
        QString eff = snap.defaultFolderPolicy;
        const auto it = snap.folders.find(QStringLiteral("mods"));
        if (it != snap.folders.end()) {
            eff = it->second;
        }
        folderEditor_->load(QStringLiteral("mods"), eff, false, QString());
        folderEditor_->setScopeTop();
        switchEditor(1);
    });

    connect(modConvertBtn, &QPushButton::clicked, this, [this]() {
        if (repoDir_.isEmpty() || branch_.isEmpty()) {
            emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
            return;
        }
        const QString modsDir = repoDir_ + QStringLiteral("/branches/")
            + branch_ + QStringLiteral("/mods");
        if (!QDir(modsDir).exists()) {
            emit logMessage(QString::fromUtf8("mods \u76ee\u5f55\u4e0d\u5b58\u5728: ") + modsDir);
            return;
        }
        pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
        pointerEditor_->batchConvertJars(modsDir);
        switchEditor(3);
    });
}

void ModpackContentIde::setRepository(const QString& repoDir)
{
    repoDir_ = repoDir;
    fileTreeSnapshot_ = QDateTime();
    refreshPoliciesView();
}

QStringList ModpackContentIde::scanBranchRootDirs() const
{
    QStringList out;
    if (repoDir_.isEmpty() || branch_.isEmpty()) return out;
    QDir dir(repoDir_ + QStringLiteral("/branches/") + branch_);
    if (!dir.exists()) return out;
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& fi : entries) {
        out << fi.fileName();
    }
    return out;
}

void ModpackContentIde::refreshPoliciesView()
{
    auto* editor = optionsPanel_ ? optionsPanel_->findChild<SyncPoliciesEditor*>(
        QStringLiteral("topSyncEditor")) : nullptr;
    if (!editor || repoDir_.isEmpty()) return;

    nlohmann::json cur = nullptr;
    QFile f(repoDir_ + QStringLiteral("/workspace.json"));
    if (f.open(QIODevice::ReadOnly)) {
        try {
            const auto j = nlohmann::json::parse(f.readAll().toStdString());
            f.close();
            if (j.contains("sync_policies")) {
                cur = j["sync_policies"];
            }
        } catch (...) {
            f.close();
        }
    }
    editor->load(cur);
    editor->setFolderCandidates(scanBranchRootDirs());
}

void ModpackContentIde::setBranch(const QString& branch, const QString& branchConfigDir)
{
    branch_ = branch;
    branchConfigDir_ = branchConfigDir;
    fileTreeSnapshot_ = QDateTime();
    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
    reloadExtraConfigFiles();

    QString branchDir;
    if (!repoDir_.isEmpty() && !branch_.isEmpty()) {
        branchDir = repoDir_ + QStringLiteral("/branches/") + branch_;
    }
    repoPanel_->setRootPath(branchDir);
    repoPanel_->setPointerDir(branchConfigDir_);
    refreshBranchMeta();
    if (auto* editor = optionsPanel_->findChild<SyncPoliciesEditor*>(
            QStringLiteral("topSyncEditor"))) {
        editor->setFolderCandidates(scanBranchRootDirs());
    }
    refreshPreview();
}

void ModpackContentIde::refreshBranchMeta()
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) {
        chain_.clear();
        return;
    }
    chain_ = buildBranchChain(repoDir_, branch_);
    const QString dir = branchDir();
    // 两个 setter 用 rebuild=false 只更新数据, 末尾统一重建一次
    // (否则每个 setter 各触发一次全量重建, commit 后 3 次重复扫描)
    repoPanel_->setInheritedFiles(collectInheritedFiles(repoDir_, chain_, dir), false);
    repoPanel_->setBranchManifest(readBranchManifest(dir), false);
    QMap<QString, QString> relToSha;
    QMap<QString, QString> relToResolver;
    collectPointerMaps(branchConfigDir_, branch_, relToSha, relToResolver);
    repoPanel_->setPointerFiles(relToSha, relToResolver, false);
    reloadExtraConfigFiles();
    repoPanel_->refresh();
}

void ModpackContentIde::injectConfigEditorExt(const QString& relPath)
{
    if (!extRegistry_) {
        configEditor_->setEditorExtension(nullptr);
        return;
    }
    const QString ext = QFileInfo(relPath).suffix().toLower();
    configEditor_->setEditorExtension(
        extRegistry_->configEditorFor(ext.isEmpty()
            ? QString() : QStringLiteral(".") + ext));
}

void ModpackContentIde::reloadExtraConfigFiles()
{
    QSet<QString> rels;
    if (!repoDir_.isEmpty()) {
        QFile f(repoDir_ + QStringLiteral("/workspace.json"));
        if (f.open(QIODevice::ReadOnly)) {
            try {
                const auto j = nlohmann::json::parse(f.readAll().toStdString());
                f.close();
                auto collect = [&rels](const nlohmann::json& sp) {
                    if (!sp.is_object()) return;
                    if (sp.contains("config_files") && sp["config_files"].is_array()) {
                        for (const auto& x : sp["config_files"]) {
                            if (x.is_string()) {
                                rels.insert(QString::fromStdString(x.get<std::string>()));
                            }
                        }
                    }
                };
                if (j.contains("sync_policies")) {
                    collect(j["sync_policies"]);
                }
                if (j.contains("branches") && j["branches"].is_array()) {
                    for (const auto& b : j["branches"]) {
                        if (!b.is_object()) continue;
                        if (b.value("name", "") != branch_.toStdString()) continue;
                        if (b.contains("sync_policies")) {
                            collect(b["sync_policies"]);
                        }
                        break;
                    }
                }
            } catch (...) {
                f.close();
            }
        }
    }
    repoPanel_->setExtraConfigFiles(rels);
    outputPanel_->setExtraConfigFiles(rels);
}

QString ModpackContentIde::trashDir() const
{
    return repoDir_ + QStringLiteral("/.NSUM/editor-trash");
}

void ModpackContentIde::onCommitFinished(const QString& branch)
{
    Q_UNUSED(branch);
    const QString trash = trashDir();
    if (QFileInfo::exists(trash)) {
        QDir(trash).removeRecursively();
    }
    const QString cache = repoDir_ + QStringLiteral("/.NSUM/pointer-cache");
    if (QFileInfo::exists(cache)) {
        QDir(cache).removeRecursively();
    }
    if (undoStack_) {
        undoStack_->clear();
    }
    // 重新扫描磁盘文件树 (commit/revert/reset 后工作区内容已变化)
    // refreshBranchMeta 内部已统一重建一次, 无需再单独 refresh
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::cleanupOnExit()
{
    const QString cache = repoDir_ + QStringLiteral("/.NSUM/pointer-cache");
    if (QFileInfo::exists(cache)) {
        QDir(cache).removeRecursively();
    }
}

void ModpackContentIde::deletePath(const QString& rel, bool isDir)
{
    if (repoDir_.isEmpty() || branch_.isEmpty() || rel.isEmpty()) return;

    if (isDir) {
        const QString base = branchDir() + QLatin1Char('/') + rel;
        QStringList files;
        QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            files << rel + QLatin1Char('/')
                + QDir(base).relativeFilePath(it.filePath());
        }
        deleteFileList(files, { base });
        return;
    }

    QStringList files;
    files << rel;
    deleteFileList(files);
}

void ModpackContentIde::deletePaths(const QList<RepoObjectInfo>& infos)
{
    if (infos.isEmpty()) return;
    QStringList files;
    QStringList dirs;
    for (const RepoObjectInfo& info : infos) {
        if (info.type == RepoObjectType::Folder) {
            const QString base = branchDir() + QLatin1Char('/') + info.path;
            QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                files << info.path + QLatin1Char('/')
                    + QDir(base).relativeFilePath(it.filePath());
            }
            dirs << base;
        } else {
            files << info.path;
        }
    }
    files.removeDuplicates();
    if (!files.isEmpty()) {
        deleteFileList(files, dirs);
    } else if (!dirs.isEmpty()) {
        deleteFileList(QStringList(), dirs);
    }
}

void ModpackContentIde::deleteFileList(const QStringList& files,
    const QStringList& dirsToRemove)
{
    if (files.isEmpty() && dirsToRemove.isEmpty()) return;

    auto state = std::make_shared<DeleteState>();
    const QString repoDir = repoDir_;
    const QString branchDir = this->branchDir();
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;
    const QStringList chain = chain_;
    const QString trash = trashDir();

    auto doDelete = [state, repoDir, branchDir, bcDir, branch, chain, trash,
                     files, dirsToRemove, this]() {
        QStringList changed;
        for (const QString& f : files) {
            const QString abs = branchDir + QLatin1Char('/') + f;
            const bool hasEntity = QFileInfo::exists(abs);
            const bool parentHas = fileExistsInParentChain(repoDir, chain, f);
            if (parentHas) {
                state->parentHasRels << f;
            }
            if (hasEntity) {
                const QString sha = computeSha256(abs);
                const QString trashAbs = trash + QLatin1Char('/') + f;
                QDir().mkpath(QFileInfo(trashAbs).absolutePath());
                QFile::remove(trashAbs);
                if (QFile::rename(abs, trashAbs)) {
                    state->movedSha[f] = sha;
                    removeFromFileManifest(bcDir, branch, f);
                    changed << abs;
                }
            }
        }
        if (!state->parentHasRels.isEmpty()) {
            auto markers = readBranchManifest(branchDir);
            for (const QString& f : state->parentHasRels) {
                markers[f] = QStringLiteral("delete");
            }
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        for (const QString& d : dirsToRemove) {
            if (QDir(d).exists()) {
                QDir(d).removeRecursively();
                changed << d;
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
        CLogger::Info("ModpackContentIde: deleted {} files, {} dirs (branch {})",
            files.size(), dirsToRemove.size(), branch.toStdString());
    };

    auto undoDelete = [state, repoDir, branchDir, bcDir, branch, chain, trash,
                       files, dirsToRemove, this]() {
        QStringList changed;
        for (const QString& d : dirsToRemove) {
            if (QDir().mkpath(d)) {
                changed << d;
            }
        }
        for (int i = files.size() - 1; i >= 0; --i) {
            const QString& f = files[i];
            const QString trashAbs = trash + QLatin1Char('/') + f;
            if (state->movedSha.contains(f)) {
                const QString abs = branchDir + QLatin1Char('/') + f;
                QDir().mkpath(QFileInfo(abs).absolutePath());
                if (QFile::rename(trashAbs, abs)) {
                    updateFileManifest(bcDir, branch, f, state->movedSha[f]);
                    changed << abs;
                }
            }
        }
        if (!state->parentHasRels.isEmpty()) {
            auto markers = readBranchManifest(branchDir);
            for (const QString& f : state->parentHasRels) {
                markers.remove(f);
            }
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    // 异步版本: 文件操作在后台线程执行 (大目录删除耗时), 进度回调驱动卡片
    auto asyncDo = [this, state, repoDir, branchDir, bcDir, branch, chain,
                    trash, files, dirsToRemove](const ProgressCb& cb) {
        const int totalN = files.size() + dirsToRemove.size();
        int idx = 0;
        QStringList changed;
        for (const QString& f : files) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u5220\u9664: %1").arg(f));
            const QString abs = branchDir + QLatin1Char('/') + f;
            const bool hasEntity = QFileInfo::exists(abs);
            const bool parentHas = fileExistsInParentChain(repoDir, chain, f);
            if (parentHas) {
                state->parentHasRels << f;
            }
            if (hasEntity) {
                const QString sha = computeSha256(abs);
                const QString trashAbs = trash + QLatin1Char('/') + f;
                QDir().mkpath(QFileInfo(trashAbs).absolutePath());
                QFile::remove(trashAbs);
                if (QFile::rename(abs, trashAbs)) {
                    state->movedSha[f] = sha;
                    removeFromFileManifest(bcDir, branch, f);
                    changed << abs;
                }
            }
            ++idx;
        }
        if (!state->parentHasRels.isEmpty()) {
            auto markers = readBranchManifest(branchDir);
            for (const QString& f : state->parentHasRels) {
                markers[f] = QStringLiteral("delete");
            }
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        for (const QString& d : dirsToRemove) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u5220\u9664\u6587\u4ef6\u5939: %1").arg(d));
            if (QDir(d).exists()) {
                QDir(d).removeRecursively();
                changed << d;
            }
            ++idx;
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
        CLogger::Info("ModpackContentIde: deleted {} files, {} dirs (branch {})",
            files.size(), dirsToRemove.size(), branch.toStdString());
    };

    auto asyncUndo = [this, state, branchDir, bcDir, branch, trash,
                      files, dirsToRemove](const ProgressCb& cb) {
        const int totalN = files.size() + dirsToRemove.size();
        int idx = 0;
        QStringList changed;
        for (const QString& d : dirsToRemove) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u6062\u590d\u6587\u4ef6\u5939: %1").arg(d));
            if (QDir().mkpath(d)) {
                changed << d;
            }
            ++idx;
        }
        for (int i = files.size() - 1; i >= 0; --i) {
            const QString& f = files[i];
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u6062\u590d: %1").arg(f));
            const QString trashAbs = trash + QLatin1Char('/') + f;
            if (state->movedSha.contains(f)) {
                const QString abs = branchDir + QLatin1Char('/') + f;
                QDir().mkpath(QFileInfo(abs).absolutePath());
                if (QFile::rename(trashAbs, abs)) {
                    updateFileManifest(bcDir, branch, f, state->movedSha[f]);
                    changed << abs;
                }
            }
            ++idx;
        }
        if (!state->parentHasRels.isEmpty()) {
            auto markers = readBranchManifest(branchDir);
            for (const QString& f : state->parentHasRels) {
                markers.remove(f);
            }
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    QString label = files.size() > 0
        ? QString::fromUtf8("\u5220\u9664 %1 \u4e2a\u6587\u4ef6").arg(files.size())
        : QString::fromUtf8("\u5220\u9664\u6587\u4ef6\u5939");
    if (!dirsToRemove.isEmpty() && files.size() > 0) {
        label = QString::fromUtf8("\u5220\u9664\u6587\u4ef6\u5939\uff08%1 \u4e2a\u6587\u4ef6\uff09")
            .arg(files.size());
    }
    undoStack_->push(new BranchFileCommand(std::move(doDelete),
        std::move(undoDelete), label, this,
        std::move(asyncDo), std::move(asyncUndo)));
    refreshBranchMeta();
    refreshPreview();
}

QStringList ModpackContentIde::infosToPaths(const QList<RepoObjectInfo>& infos)
{
    QStringList paths;
    for (const RepoObjectInfo& info : infos) {
        if (!info.path.isEmpty()) {
            paths << info.path;
        }
    }
    return paths;
}

void ModpackContentIde::pasteBranchFiles(const QStringList& relPaths,
    const QString& targetRel, bool isCut)
{
    if (repoDir_.isEmpty() || branch_.isEmpty() || relPaths.isEmpty()) return;

    struct PasteOp {
        QString src;
        QString dst;
        bool wasCut = false;
    };
    auto ops = std::make_shared<QVector<PasteOp>>();

    const QString branchDir = this->branchDir();
    QStringList conflicts;
    QStringList existRels;
    for (const QString& rel : relPaths) {
        QString srcAbs = branchDir + QLatin1Char('/') + rel;
        bool srcInherited = false;
        if (!QFileInfo::exists(srcAbs)) {
            const QString parentAbs = parentEntityPath(rel);
            if (parentAbs.isEmpty()) continue;
            srcAbs = parentAbs;
            srcInherited = true;
        }
        QString fileName = QFileInfo(rel).fileName();
        if (fileName.isEmpty()) fileName = QFileInfo(srcAbs).fileName();
        QString dstRel = targetRel;
        if (!dstRel.isEmpty()) dstRel += QLatin1Char('/');
        dstRel += fileName;
        const QString dstAbs = branchDir + QLatin1Char('/') + dstRel;
        if (srcAbs == dstAbs) continue;
        PasteOp op;
        op.src = srcAbs;
        op.dst = dstAbs;
        op.wasCut = isCut && !srcInherited;
        ops->push_back(op);
        if (QFileInfo::exists(dstAbs)) {
            existRels << dstRel;
        }
    }
    if (ops->isEmpty()) {
        emit logMessage(QString::fromUtf8("\u7c98\u8d34\u5931\u8d25: \u6e90\u6587\u4ef6\u4e0d\u5b58\u5728\u3002"));
        return;
    }

    if (!existRels.isEmpty()) {
        QMessageBox box(this);
        box.setWindowTitle(QString::fromUtf8("\u76ee\u6807\u5b58\u5728\u51b2\u7a81"));
        box.setText(QString::fromUtf8("%1 \u4e2a\u76ee\u6807\u6587\u4ef6\u5df2\u5b58\u5728\uff0c\u662f\u5426\u8986\u76d6\uff1f")
            .arg(existRels.size()));
        box.setInformativeText(existRels.first(3).join(QLatin1Char('\n'))
            + (existRels.size() > 3
                ? QString::fromUtf8("\n\u2026\u7b49 %1 \u9879").arg(existRels.size() - 3)
                : QString()));
        box.setIcon(QMessageBox::Warning);
        QPushButton* okBtn = box.addButton(QString::fromUtf8("\u8986\u76d6"),
            QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() != okBtn) {
            emit logMessage(QString::fromUtf8("\u5df2\u53d6\u6d88\u7c98\u8d34\u3002"));
            return;
        }
    }

    const QString repoDir = repoDir_;
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;

    auto failed = std::make_shared<QStringList>();

    auto doPaste = [ops, branchDir, failed, this]() {
        QStringList changed;
        for (const PasteOp& op : *ops) {
            QDir().mkpath(QFileInfo(op.dst).absolutePath());
            // 覆盖前先备份, 失败时恢复原文件
            const bool hadDst = QFile::exists(op.dst);
            const QString backup = op.dst + QStringLiteral(".nsum-bak");
            if (hadDst) {
                QFile::remove(backup);
                QFile::rename(op.dst, backup);
            }
            bool ok = false;
            if (op.wasCut) {
                ok = QFile::rename(op.src, op.dst);
            } else {
                ok = QFile::copy(op.src, op.dst);
            }
            if (ok) {
                QFile::remove(backup);
                changed << op.dst;
            } else {
                if (hadDst) {
                    QFile::rename(backup, op.dst);
                }
                failed->append(op.dst);
                CLogger::Error("Paste failed: {} -> {}", op.src.toStdString(),
                    op.dst.toStdString());
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto undoPaste = [ops, branchDir, this]() {
        QStringList changed;
        for (int i = ops->size() - 1; i >= 0; --i) {
            const PasteOp& op = (*ops)[i];
            QFile::remove(op.dst);
            if (op.wasCut) {
                QDir().mkpath(QFileInfo(op.src).absolutePath());
                if (QFile::rename(op.dst, op.src)) {
                    changed << op.src;
                }
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    // 异步版本: 后台线程执行文件复制/移动, 失败收集在线程内, 完成回调弹窗
    auto asyncDo = [this, ops, branchDir, failed](const ProgressCb& cb) {
        const int totalN = ops->size();
        int idx = 0;
        QStringList changed;
        for (const PasteOp& op : *ops) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u7c98\u8d34: %1").arg(QFileInfo(op.dst).fileName()));
            QDir().mkpath(QFileInfo(op.dst).absolutePath());
            const bool hadDst = QFile::exists(op.dst);
            const QString backup = op.dst + QStringLiteral(".nsum-bak");
            if (hadDst) {
                QFile::remove(backup);
                QFile::rename(op.dst, backup);
            }
            bool ok = false;
            if (op.wasCut) {
                ok = QFile::rename(op.src, op.dst);
            } else {
                ok = QFile::copy(op.src, op.dst);
            }
            if (ok) {
                QFile::remove(backup);
                changed << op.dst;
            } else {
                if (hadDst) {
                    QFile::rename(backup, op.dst);
                }
                failed->append(op.dst);
                CLogger::Error("Paste failed: {} -> {}", op.src.toStdString(),
                    op.dst.toStdString());
            }
            ++idx;
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto asyncUndo = [this, ops, branchDir](const ProgressCb& cb) {
        const int totalN = ops->size();
        int idx = 0;
        QStringList changed;
        for (int i = ops->size() - 1; i >= 0; --i) {
            const PasteOp& op = (*ops)[i];
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u64a4\u9500: %1").arg(QFileInfo(op.dst).fileName()));
            QFile::remove(op.dst);
            if (op.wasCut) {
                QDir().mkpath(QFileInfo(op.src).absolutePath());
                if (QFile::rename(op.dst, op.src)) {
                    changed << op.src;
                }
            }
            ++idx;
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto asyncFinish = [this, failed]() {
        if (failed->isEmpty()) return;
        QMessageBox::warning(this, "粘贴失败",
            QString("%1 \u9879\u7c98\u8d34\u5931\u8d25\uff0c\u5df2\u6062\u590d\u539f\u76ee\u6807\u6587\u4ef6:\n%2")
                .arg(failed->size())
                .arg(failed->first(5).join(QLatin1Char('\n'))
                    + (failed->size() > 5 ? QStringLiteral("\n\u2026") : QString())));
    };

    undoStack_->push(new BranchFileCommand(std::move(doPaste),
        std::move(undoPaste),
        isCut ? QString::fromUtf8("\u526a\u5207\u7c98\u8d34 %1 \u4e2a\u9879\u76ee")
              : QString::fromUtf8("\u590d\u5236\u7c98\u8d34 %1 \u4e2a\u9879\u76ee")
                    .arg(ops->size()),
        this, std::move(asyncDo), std::move(asyncUndo),
        std::move(asyncFinish)));
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::batchRestorePointers(const QList<RepoObjectInfo>& infos)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;

    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
    auto items = std::make_shared<QVector<ConvertedItem>>();
    int okCount = 0;
    for (const RepoObjectInfo& info : infos) {
        if (info.type != RepoObjectType::Pointer || info.pointerSha.isEmpty()) {
            continue;
        }
        const QString sha = info.pointerSha;
        NeoCore::PointerFileData pfd;
        if (!readPointerFile(branchConfigDir_, sha, pfd)) continue;
        if (pfd.resolvers.empty() || pfd.original_names.empty()) continue;

        QString rel = QString::fromStdString(pfd.original_names[0]);
        {
            const QString filePath = branchConfigDir_ + QLatin1Char('/')
                + branch_ + QStringLiteral(".json");
            QFile f(filePath);
            if (f.open(QIODevice::ReadOnly)) {
                try {
                    const auto j = nlohmann::json::parse(
                        f.readAll().toStdString());
                    f.close();
                    if (j.contains("file_manifest")
                        && j["file_manifest"].is_object()) {
                        for (auto it = j["file_manifest"].begin();
                            it != j["file_manifest"].end(); ++it) {
                            if (it.value().is_string()
                                && it.value().get<std::string>()
                                    == sha.toStdString()) {
                                rel = QString::fromStdString(it.key());
                                break;
                            }
                        }
                    }
                } catch (...) {
                    f.close();
                }
            }
        }

        if (pointerEditor_->restorePointerFromCache(sha, pfd)) {
            ++okCount;
            ConvertedItem item;
            item.sha = sha;
            item.relPath = rel;
            item.cacheAbs = repoDir_ + QStringLiteral("/.NSUM/pointer-cache/")
                + rel;
            item.pointerJson = QString::fromStdString(pfd.toJson().dump(2));
            items->push_back(item);
        }
    }
    if (items->isEmpty()) return;
    emit logMessage(QString::fromUtf8("\u6279\u91cf\u8f6c\u56de\u5b8c\u6210: %1 / %2")
        .arg(okCount).arg(items->size()));

    // 入撤销栈: redo=恢复文件, undo=转回指针 (与 startBatchConvert 复用同源逻辑)
    undoStack_->push(new BranchFileCommand(
        [this, items]() {
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->undoBatchConvert(*items);
        },
        [this, items]() {
            pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
            pointerEditor_->redoBatchConvert(*items);
        },
        QString::fromUtf8("\u6279\u91cf\u8f6c\u56de\u539f\u59cb\u6587\u4ef6 (%1 \u9879)")
            .arg(items->size())));
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::restoreInherited(const QString& rel)
{
    if (repoDir_.isEmpty() || branch_.isEmpty() || rel.isEmpty()) return;

    auto state = std::make_shared<RestoreState>();
    state->rel = rel;
    const QString repoDir = repoDir_;
    const QString branchDir = this->branchDir();
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;
    const QString trash = trashDir();

    auto doRestore = [state, repoDir, branchDir, bcDir, branch, trash, this]() {
        auto markers = readBranchManifest(branchDir);
        const QString marker = markers.value(state->rel);
        if (marker.isEmpty()) return;
        state->marker = marker;

        QStringList changed;
        if (marker == QLatin1String("override")) {
            const QString abs = branchDir + QLatin1Char('/') + state->rel;
            if (QFileInfo::exists(abs)) {
                const QString sha = computeSha256(abs);
                const QString trashAbs = trash + QLatin1Char('/') + state->rel;
                QDir().mkpath(QFileInfo(trashAbs).absolutePath());
                QFile::remove(trashAbs);
                if (QFile::rename(abs, trashAbs)) {
                    state->movedEntity = true;
                    state->trashRel = trashAbs;
                    state->shaBefore = sha;
                    removeFromFileManifest(bcDir, branch, state->rel);
                    changed << abs;
                }
            }
        }
        markers.remove(state->rel);
        writeBranchManifest(branchDir, markers);
        changed << branchDir + QStringLiteral("/branch_manifest.json");
        emit gitAddRequested(changed);
    };

    auto undoRestore = [state, branchDir, bcDir, branch, this]() {
        QStringList changed;
        if (state->movedEntity) {
            const QString abs = branchDir + QLatin1Char('/') + state->rel;
            if (QFile::rename(state->trashRel, abs)) {
                updateFileManifest(bcDir, branch, state->rel, state->shaBefore);
                changed << abs;
            }
        }
        if (!state->marker.isEmpty()) {
            auto markers = readBranchManifest(branchDir);
            markers[state->rel] = state->marker;
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    undoStack_->push(new BranchFileCommand(std::move(doRestore),
        std::move(undoRestore),
        QString::fromUtf8("\u8fd8\u539f\u7236\u7248\u672c: %1").arg(rel)));
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::pushImportUndo(
    const QStringList& copiedAbs,
    const std::shared_ptr<QVector<OverwriteItem>>& overwritten,
    const QVector<ImportJob>& jobs)
{
    const int overwriteCount = (overwritten && !overwritten->isEmpty())
        ? static_cast<int>(overwritten->size()) : 0;
    if (copiedAbs.isEmpty() && overwriteCount == 0) return;

    const QString branchDir = this->branchDir();
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;

    // rel → 导入源路径映射 (redo 重新复制); 本次实际复制成功的 rel 列表
    auto srcByRel = std::make_shared<QMap<QString, QString>>();
    QStringList copiedRels;
    for (const ImportJob& job : jobs) {
        srcByRel->insert(job.rel, job.src);
    }
    const QString prefix = branchDir + QLatin1Char('/');
    for (const QString& abs : copiedAbs) {
        if (abs.startsWith(prefix)) {
            copiedRels << abs.mid(prefix.length());
        }
    }
    if (copiedRels.isEmpty() && overwriteCount == 0) return;

    auto doFn = [copiedRels, srcByRel, overwritten, branchDir, bcDir,
                 branch, this]() {
        QStringList changed;
        for (const QString& rel : copiedRels) {
            const QString dst = branchDir + QLatin1Char('/') + rel;
            if (QFile::exists(dst)) continue;
            const QString src = srcByRel->value(rel);
            if (src.isEmpty() || !QFile::exists(src)) continue;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            if (QFile::copy(src, dst)) {
                const QString sha = computeSha256(dst);
                if (sha.length() == 64) {
                    updateFileManifest(bcDir, branch, rel, sha);
                }
                changed << dst;
            }
        }
        if (overwritten) {
            for (const auto& item : *overwritten) {
                const QString dst = branchDir + QLatin1Char('/') + item.rel;
                const QString src = srcByRel->value(item.rel);
                if (!src.isEmpty() && QFile::exists(src)) {
                    QFile::remove(dst);
                    if (QFile::copy(src, dst)) {
                        const QString sha = computeSha256(dst);
                        if (sha.length() == 64) {
                            updateFileManifest(bcDir, branch, item.rel, sha);
                        }
                        changed << dst;
                    }
                }
                if (item.parentHas) {
                    auto markers = readBranchManifest(branchDir);
                    markers[item.rel] = QStringLiteral("override");
                    writeBranchManifest(branchDir, markers);
                    changed << branchDir + QStringLiteral("/branch_manifest.json");
                }
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto undoFn = [copiedRels, overwritten, branchDir, bcDir, branch, this]() {
        QStringList changed;
        for (const QString& rel : copiedRels) {
            const QString dst = branchDir + QLatin1Char('/') + rel;
            QFile::remove(dst);
            removeFromFileManifest(bcDir, branch, rel);
            changed << dst;
        }
        if (overwritten) {
            for (int i = overwritten->size() - 1; i >= 0; --i) {
                const OverwriteItem& item = (*overwritten)[i];
                const QString dst = branchDir + QLatin1Char('/') + item.rel;
                QFile::remove(dst);
                if (item.movedOld) {
                    if (QFile::rename(item.trashAbs, dst)) {
                        updateFileManifest(bcDir, branch, item.rel, item.shaBefore);
                        changed << dst;
                    }
                }
                if (item.parentHas) {
                    auto markers = readBranchManifest(branchDir);
                    markers.remove(item.rel);
                    writeBranchManifest(branchDir, markers);
                    changed << branchDir + QStringLiteral("/branch_manifest.json");
                }
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    // 异步版本: 同一逻辑在后台线程执行 (耗时撤销/重做), 进度回调驱动卡片
    auto asyncDo = [this, copiedRels, srcByRel, overwritten, branchDir,
                    bcDir, branch](const ProgressCb& cb) {
        const int totalN = copiedRels.size()
            + (overwritten ? static_cast<int>(overwritten->size()) : 0);
        int idx = 0;
        QStringList changed;
        for (const QString& rel : copiedRels) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u6062\u590d: %1").arg(rel));
            const QString dst = branchDir + QLatin1Char('/') + rel;
            if (QFile::exists(dst)) continue;
            const QString src = srcByRel->value(rel);
            if (src.isEmpty() || !QFile::exists(src)) continue;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            if (QFile::copy(src, dst)) {
                const QString sha = computeSha256(dst);
                if (sha.length() == 64) {
                    updateFileManifest(bcDir, branch, rel, sha);
                }
                changed << dst;
            }
            ++idx;
        }
        if (overwritten) {
            for (const auto& item : *overwritten) {
                cb(idx * 100 / qMax(1, totalN),
                    QString::fromUtf8("\u6062\u590d: %1").arg(item.rel));
                const QString dst = branchDir + QLatin1Char('/') + item.rel;
                const QString src = srcByRel->value(item.rel);
                if (!src.isEmpty() && QFile::exists(src)) {
                    QFile::remove(dst);
                    if (QFile::copy(src, dst)) {
                        const QString sha = computeSha256(dst);
                        if (sha.length() == 64) {
                            updateFileManifest(bcDir, branch, item.rel, sha);
                        }
                        changed << dst;
                    }
                }
                if (item.parentHas) {
                    auto markers = readBranchManifest(branchDir);
                    markers[item.rel] = QStringLiteral("override");
                    writeBranchManifest(branchDir, markers);
                    changed << branchDir + QStringLiteral("/branch_manifest.json");
                }
                ++idx;
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto asyncUndo = [this, copiedRels, overwritten, branchDir,
                      bcDir, branch](const ProgressCb& cb) {
        const int totalN = copiedRels.size()
            + (overwritten ? static_cast<int>(overwritten->size()) : 0);
        int idx = 0;
        QStringList changed;
        for (const QString& rel : copiedRels) {
            cb(idx * 100 / qMax(1, totalN),
                QString::fromUtf8("\u64a4\u9500: %1").arg(rel));
            const QString dst = branchDir + QLatin1Char('/') + rel;
            QFile::remove(dst);
            removeFromFileManifest(bcDir, branch, rel);
            changed << dst;
            ++idx;
        }
        if (overwritten) {
            for (int i = overwritten->size() - 1; i >= 0; --i) {
                const OverwriteItem& item = (*overwritten)[i];
                cb(idx * 100 / qMax(1, totalN),
                    QString::fromUtf8("\u64a4\u9500: %1").arg(item.rel));
                const QString dst = branchDir + QLatin1Char('/') + item.rel;
                QFile::remove(dst);
                if (item.movedOld) {
                    if (QFile::rename(item.trashAbs, dst)) {
                        updateFileManifest(bcDir, branch, item.rel, item.shaBefore);
                        changed << dst;
                    }
                }
                if (item.parentHas) {
                    auto markers = readBranchManifest(branchDir);
                    markers.remove(item.rel);
                    writeBranchManifest(branchDir, markers);
                    changed << branchDir + QStringLiteral("/branch_manifest.json");
                }
                ++idx;
            }
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    undoStack_->push(new BranchFileCommand(std::move(doFn), std::move(undoFn),
        QString::fromUtf8("\u5bfc\u5165 %1 \u4e2a\u6587\u4ef6")
            .arg(copiedRels.size() + overwriteCount),
        this, std::move(asyncDo), std::move(asyncUndo)));
}

QString ModpackContentIde::parentEntityPath(const QString& rel) const
{
    for (int i = chain_.size() - 2; i >= 0; --i) {
        const QString dir = repoDir_ + QStringLiteral("/branches/")
            + chain_[i];
        if (QFileInfo::exists(dir + QLatin1Char('/') + rel)) {
            return dir + QLatin1Char('/') + rel;
        }
        if (QFileInfo::exists(dir + QStringLiteral("/.overrides/") + rel)) {
            return dir + QStringLiteral("/.overrides/") + rel;
        }
    }
    return QString();
}

void ModpackContentIde::openContentEditor(const RepoObjectInfo& info)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    const QString abs = branchDir() + QLatin1Char('/') + info.path;
    QString parentAbs;
    const bool inherited = info.isInherited;
    if (inherited) {
        parentAbs = parentEntityPath(info.path);
        if (parentAbs.isEmpty()) {
            emit logMessage(
                QString::fromUtf8("\u672a\u627e\u5230\u7236\u5206\u652f\u5b9e\u4f53: %1")
                    .arg(info.path));
            switchEditor(0);
            return;
        }
    } else if (!QFile::exists(abs)) {
        switchEditor(0);
        return;
    }
    auto* contentEditor = editorStack_->findChild<FileContentEditor*>(
        QStringLiteral("fileContentEditor"));
    if (!contentEditor) {
        switchEditor(0);
        return;
    }
    contentEditor->loadContent(info.path, abs, inherited, parentAbs);
    switchEditor(5);
}

void ModpackContentIde::onContentSave(const QString& relPath,
    const QString& content, bool inherited)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    const QString abs = branchDir() + QLatin1Char('/') + relPath;
    QDir().mkpath(QFileInfo(abs).absolutePath());
    {
        QFile f(abs);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            emit logMessage(QString::fromUtf8("\u5199\u5165\u5931\u8d25: %1").arg(relPath));
            return;
        }
        f.write(content.toUtf8());
        f.close();
    }
    const QString sha = computeSha256(abs);
    if (sha.length() == 64) {
        updateFileManifest(branchConfigDir_, branch_, relPath, sha);
    }

    QStringList changed = { abs };
    if (inherited) {
        const QString branchDir = this->branchDir();
        const QString bcDir = branchConfigDir_;
        const QString branch = branch_;
        const bool parentHas = fileExistsInParentChain(repoDir_, chain_, relPath);
        if (parentHas) {
            auto markers = readBranchManifest(branchDir);
            markers[relPath] = QStringLiteral("override");
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }

        auto doFn = []() {};
        auto undoFn = [relPath, abs, bcDir, branch, branchDir, this]() {
            QFile::remove(abs);
            removeFromFileManifest(bcDir, branch, relPath);
            auto markers = readBranchManifest(branchDir);
            markers.remove(relPath);
            writeBranchManifest(branchDir, markers);
            emit gitAddRequested({
                abs,
                branchDir + QStringLiteral("/branch_manifest.json"),
            });
        };
        undoStack_->push(new BranchFileCommand(std::move(doFn),
            std::move(undoFn),
            QString::fromUtf8("\u7f16\u8f91\u7ee7\u627f\u6587\u4ef6: %1").arg(relPath)));
    }
    emit gitAddRequested(changed);
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::undoBranchOp()
{
    // 多卡并存: 每个命令独立线程执行, 无需全局拦截 (栈保证单步语义)
    if (undoStack_->canUndo()) {
        undoStack_->undo();
        refreshBranchMeta();
        refreshPreview();
    }
}

void ModpackContentIde::redoBranchOp()
{
    if (undoStack_->canRedo()) {
        undoStack_->redo();
        refreshBranchMeta();
        refreshPreview();
    }
}

void ModpackContentIde::runAsyncCommand(const QString& title,
    const std::function<void(const std::function<void(int, const QString&)>&)>& work,
    const std::function<void()>& onFinished)
{
    if (!work) return;

    // 每项操作一张独立卡片 (右上角层叠堆, 可多卡并存), 完成才关闭
    auto* card = spawnWorkCard(title, false);
    QApplication::processEvents();

    QPointer<ModpackContentIde> guard = this;
    QPointer<HiBerGUI::WorkCard> cardGuard = card;
    auto thread = std::make_unique<std::thread>([guard, cardGuard, work, onFinished]() {
        if (guard.isNull()) return;
        // 后台执行文件操作; 进度回调跨线程排队回 GUI 更新卡片
        work([guard, cardGuard](int p, const QString& s) {
            QMetaObject::invokeMethod(guard.data(), [guard, cardGuard, p, s]() {
                if (guard.isNull() || cardGuard.isNull()) return;
                cardGuard->setProgress(p, s);
            }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(guard.data(), [guard, cardGuard, onFinished]() {
            if (guard.isNull()) return;
            if (cardGuard) {
                cardGuard->complete(QString());
                guard->removeWorkCard(cardGuard.data());
            }
            if (onFinished) {
                onFinished();
            }
            guard->refreshBranchMeta();
            guard->refreshPreview();
            if (guard->repoPanel_) {
                guard->repoPanel_->refresh();
            }
        }, Qt::QueuedConnection);
    });
    workThreads_.push_back(std::move(thread));
}

QWidget* ModpackContentIde::activeLeftPanel() const
{
    if (!leftTabs_) return repoPanel_;
    const QWidget* w = leftTabs_->currentWidget();
    if (w == outputPanel_) return outputPanel_;
    if (w == repoPanel_) return repoPanel_;
    return nullptr;
}

void ModpackContentIde::deleteCurrentSelection()
{
    if (activeLeftPanel() == outputPanel_) {
        const RepoObjectInfo info = outputPanel_->currentSelection();
        if (info.type == RepoObjectType::Root || info.path.isEmpty()) {
            emit logMessage(
                QString::fromUtf8("\u8bf7\u5148\u5728\u6587\u4ef6\u6811\u4e2d\u9009\u62e9\u9879\u76ee\u3002"));
            return;
        }
        if (info.marker == QLatin1String("D")) {
            emit logMessage(QString::fromUtf8(
                "\u8be5\u6587\u4ef6\u5df2\u4ece\u6253\u5305\u4e2d\u5220\u9664\u3002"));
            return;
        }
        deletePath(info.path, info.type == RepoObjectType::Folder);
        return;
    }
    const RepoObjectInfo info = repoPanel_->currentSelection();
    if (info.type == RepoObjectType::Root || info.path.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u8bf7\u5148\u5728\u4ed3\u5e93\u6587\u4ef6\u6811\u4e2d\u9009\u62e9\u9879\u76ee\u3002"));
        return;
    }
    deletePath(info.path, info.type == RepoObjectType::Folder);
}

void ModpackContentIde::restoreCurrentSelection()
{
    if (activeLeftPanel() != repoPanel_) {
        emit logMessage(QString::fromUtf8(
            "\u8fd8\u539f\u7236\u7248\u672c\u4ec5\u652f\u6301\u4ed3\u5e93\u6587\u4ef6\u6811\u3002"));
        return;
    }
    const RepoObjectInfo info = repoPanel_->currentSelection();
    if (info.type == RepoObjectType::Root || info.path.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u8bf7\u5148\u5728\u4ed3\u5e93\u6587\u4ef6\u6811\u4e2d\u9009\u62e9\u9879\u76ee\u3002"));
        return;
    }
    if (!info.isInherited && info.marker.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u8be5\u9879\u76ee\u6ca1\u6709\u6807\u8bb0\uff0c\u65e0\u9700\u8fd8\u539f\u3002"));
        return;
    }
    restoreInherited(info.path);
}

void ModpackContentIde::createBranchFolder(const QString& parentRel)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
        return;
    }
    QString absParent = branchDir();
    if (!parentRel.isEmpty()) {
        absParent += QLatin1Char('/') + parentRel;
    }
    if (HiBerGUI::createFolderInteractive(this, absParent)) {
        refreshBranchMeta();
        refreshPreview();
    }
}

void ModpackContentIde::createServerConfigFolder()
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
        return;
    }
    // save = 存档文件夹; [save] = 单个存档目录占位 (名称任意); serverconfig = 同步目标
    const QString dir = branchDir() + QStringLiteral("/save/[save]/serverconfig");
    if (QDir(dir).exists()) {
        emit logMessage(QString::fromUtf8(
            "\u5df2\u5b58\u5728: %1").arg(dir));
        return;
    }
    if (QDir().mkpath(dir)) {
        // 立刻缓存配置: 写入默认规则配置 (globle.json 默认模式 / list.json 文件清单)
        const QString ruleDir = dir + QStringLiteral("/.rule");
        QDir().mkpath(ruleDir);
        {
            nlohmann::json globle;
            globle["default_mode"] = "full";
            globle["folder_mode"] = "mirror";
            std::ofstream f((ruleDir + QStringLiteral("/globle.json")).toStdString());
            if (f.is_open()) {
                f << globle.dump(2) << std::endl;
                f.close();
            }
        }
        {
            nlohmann::json list;
            list["files"] = nlohmann::json::object();
            std::ofstream f((ruleDir + QStringLiteral("/list.json")).toStdString());
            if (f.is_open()) {
                f << list.dump(2) << std::endl;
                f.close();
            }
        }
        // 立刻增加 Git 追踪
        emit gitAddRequested({ dir });
        emit logMessage(QString::fromUtf8(
            "\u2705 \u5df2\u521b\u5efa serverconfig \u540c\u6b65\u6587\u4ef6\u5939: %1").arg(dir));
        refreshBranchMeta();
        refreshPreview();
    } else {
        emit logMessage(QString::fromUtf8(
            "\u2718 \u521b\u5efa\u5931\u8d25: %1").arg(dir));
    }
}

QDateTime ModpackContentIde::lastModifiedOf(const QString& dirPath) const
{
    QDir dir(dirPath);
    if (!dir.exists()) return QDateTime();

    QDateTime latest = QFileInfo(dirPath).lastModified();
    const QStringList skip = { QStringLiteral(".git"),
        QStringLiteral(".NSUM") };
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::Files
        | QDir::NoDotAndDotDot, QDir::DirsFirst);
    for (const auto& fi : entries) {
        if (fi.isDir() && skip.contains(fi.fileName())) continue;
        QDateTime t = fi.lastModified();
        if (fi.isDir()) {
            const QDateTime sub = lastModifiedOf(fi.absoluteFilePath());
            if (sub > t) t = sub;
        }
        if (t > latest) latest = t;
    }
    return latest;
}

void ModpackContentIde::refreshFileTree()
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    if (importRunning_.load() || previewRunning_.load()) return;
    if (!repoPanel_) return;

    // 无变动不重绘: 对比分支目录最后修改时间快照, 无变化则跳过 (避免白展开)
    const QString branchDirPath = branchDir();
    const QDateTime mtime = lastModifiedOf(branchDirPath);
    if (mtime == fileTreeSnapshot_) return;
    fileTreeSnapshot_ = mtime;
    repoPanel_->refresh();
}

void ModpackContentIde::refreshPreview()
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) {
        outputPanel_->setStatusText(
            QString::fromUtf8("\u672a\u6253\u5f00\u4ed3\u5e93\u6216\u672a\u9009\u62e9\u5206\u652f\u3002"));
        return;
    }

    if (previewRunning_.load()) {
        if (cancelToken_) {
            cancelToken_->request_cancel();
        }
        if (previewThread_ && previewThread_->joinable()) {
            previewThread_->join();
        }
        previewThread_.reset();
        previewRunning_.store(false);
    }

    const QString targetDir = repoDir_ + QStringLiteral("/.minecraft/versions/") + branch_;
    const QString format = outputPanel_->format();
    const int generation = ++previewGeneration_;

    std::vector<NeoBuild::BranchLayer> layers;
    QStringList chain = chain_;
    if (chain.isEmpty()) {
        chain = buildBranchChain(repoDir_, branch_);
        chain_ = chain;
    }
    if (chain.isEmpty()) {
        chain << branch_;
    }
    for (const QString& name : chain) {
        NeoBuild::BranchLayer layer;
        layer.name = name.toStdString();
        layer.baseDir = (repoDir_ + QStringLiteral("/branches/")
            + name).toStdString();
        layer.overridesDir = layer.baseDir + "/.overrides";
        layer.manifest = NeoBuild::BranchMerger::loadManifest(layer.baseDir);
        layers.push_back(std::move(layer));
    }

    cancelToken_ = std::make_unique<NeoCore::CancelToken>();
    NeoCore::CancelToken* cancel = cancelToken_.get();

    auto* card = spawnWorkCard(
        QString::fromUtf8("\u751f\u6210\u9884\u89c8 (%1 / %2)").arg(branch_, format), true);
    connect(card, &HiBerGUI::WorkCard::cancelRequested, this, [this]() {
        if (cancelToken_) {
            cancelToken_->request_cancel();
        }
    });
    previewRunning_.store(true);

    auto* self = this;
    QPointer<HiBerGUI::WorkCard> cardGuard = card;
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;
    previewThread_ = std::make_unique<std::thread>(
        [self, cardGuard, generation, layers = std::move(layers), targetDir, cancel,
         bcDir, branch]() {
        const bool compare = QDir(targetDir).exists();
        const std::string targetDirStd = compare ? targetDir.toStdString() : "";

        nlohmann::json entries = NeoBuild::generateUmdStructureFromLayers(
            layers, targetDirStd, nullptr, cancel);
        const bool cancelled = cancel->is_cancelled();

        // 指针文件注入: 显示于最终输出位置 (三种输出格式统一), 颜色标记
        QMap<QString, QString> relToSha, relToResolver;
        collectPointerMaps(bcDir, branch, relToSha, relToResolver);
        QSet<QString> pointerRels;
        if (entries.is_array()) {
            QSet<QString> existing;
            for (const auto& e : entries) {
                if (e.is_object()) {
                    existing.insert(QString::fromStdString(
                        e.value("path", std::string())));
                }
            }
            for (auto it = relToSha.begin(); it != relToSha.end(); ++it) {
                pointerRels.insert(it.key());
                if (!existing.contains(it.key())) {
                    nlohmann::json e;
                    e["path"] = it.key().toStdString();
                    e["dir"] = false;
                    e["umd"] = "";
                    entries.push_back(e);
                }
            }
        }

        QMetaObject::invokeMethod(self, [self, cardGuard, generation, entries,
            pointerRels, cancelled]() {
            if (generation != self->previewGeneration_) {
                // 过期预览: 必须移除其卡片, 否则"假刷新任务"永久卡死
                self->previewRunning_.store(false);
                if (cardGuard) {
                    self->removeWorkCard(cardGuard.data());
                }
                return;
            }
            self->outputPanel_->loadEntries(entries, pointerRels);
            if (cardGuard) {
                cardGuard->complete(cancelled
                    ? QString::fromUtf8("\u5df2\u53d6\u6d88\uff0c\u663e\u793a\u90e8\u5206\u7ed3\u679c\u3002")
                    : QString::fromUtf8("\u9884\u89c8\u751f\u6210\u5b8c\u6210\u3002"));
                self->removeWorkCard(cardGuard.data());
            }
            self->previewRunning_.store(false);
            if (self->previewThread_ && self->previewThread_->joinable()) {
                self->previewThread_->join();
            }
            self->previewThread_.reset();
        }, Qt::QueuedConnection);
    });
}

QString ModpackContentIde::branchDir() const
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return QString();
    return repoDir_ + QStringLiteral("/branches/") + branch_;
}

void ModpackContentIde::applyFolderPolicyToSubfolders(const QString& folderPath,
    const QString& policy, bool toBranch)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    const QString base = branchDir();
    QString clean = QDir::cleanPath(folderPath);
    if (clean == QStringLiteral(".")) clean.clear();

    // 扫描当前文件夹下所有子目录 (跳过与仓库树一致的内部目录)
    static const QStringList skipDirs = {
        QStringLiteral(".git"), QStringLiteral(".NSUM"),
        QStringLiteral(".rule"), QStringLiteral("branch_config"),
        QStringLiteral(".overrides"),
    };
    QStringList subdirs;
    const QString scanRoot = clean.isEmpty()
        ? base : base + QLatin1Char('/') + clean;
    QDirIterator it(scanRoot, QDir::Dirs | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString rel = QDir(base).relativeFilePath(it.next());
        rel.replace(QLatin1Char('\\'), QLatin1Char('/'));
        bool skip = false;
        for (const QString& seg : rel.split(QLatin1Char('/'))) {
            if (skipDirs.contains(seg)) {
                skip = true;
                break;
            }
        }
        if (!skip) {
            subdirs << rel;
        }
    }
    if (subdirs.isEmpty()) {
        emit logMessage(QString::fromUtf8(
            "\u8be5\u6587\u4ef6\u5939\u4e0b\u672a\u627e\u5230\u5b50\u6587\u4ef6\u5939\u3002"));
        return;
    }
    const auto reply = QMessageBox::question(this,
        QString::fromUtf8("\u5e94\u7528\u5230\u5b50\u6587\u4ef6\u5939"),
        QString::fromUtf8(
            "\u5c06\u628a\u5f53\u524d\u7b56\u7565\u5e94\u7528\u5230 %1 \u4e0b\u7684 %2 \u4e2a\u5b50\u6587\u4ef6\u5939\uff0c\u662f\u5426\u7ee7\u7eed\uff1f")
            .arg(clean.isEmpty() ? QStringLiteral("/") : clean)
            .arg(subdirs.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    for (const QString& rel : subdirs) {
        emit folderPolicySaveRequested(rel, policy, toBranch);
    }
    refreshFolderEditorState();
}

void ModpackContentIde::refreshFolderEditorState()
{
    if (!folderEditor_ || repoDir_.isEmpty() || branch_.isEmpty()) return;
    if (editorStack_->currentWidget() != folderEditor_) return;
    const QString path = folderEditor_->currentFolderPath();
    if (path.isEmpty()) return;
    const PolicySnapshot snap = parsePolicies(repoDir_, branch_);
    const auto it = snap.folders.find(path);
    const bool overrides = snap.branchFolderOverrides.count(path) > 0;
    const QString effective = (it != snap.folders.end())
        ? it->second : snap.defaultFolderPolicy;
    folderEditor_->load(path, effective, overrides, branch_);
}

void ModpackContentIde::showDropTargetHint(const QString& targetRel, bool hovering)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    if (!hovering) {
        emit logMessage(QString());
        return;
    }
    if (targetRel.isEmpty()) {
        emit logMessage(QString::fromUtf8(
            "\u5c06\u5bfc\u5165\u5230: branches/%1/\uff08\u5206\u652f\u6839\u76ee\u5f55\uff09")
            .arg(branch_));
    } else {
        emit logMessage(QString::fromUtf8("\u5c06\u5bfc\u5165\u5230: branches/%1/%2")
            .arg(branch_).arg(targetRel));
    }
}

void ModpackContentIde::importDroppedFiles(const QStringList& filePaths,
    const QString& targetRel)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) {
        emit logMessage(QString::fromUtf8(
            "\u8bf7\u5148\u6253\u5f00\u4ed3\u5e93\u5e76\u9009\u62e9\u5206\u652f\u3002"));
        return;
    }
    if (importRunning_.load()) {
        emit logMessage(QString::fromUtf8(
            "\u5bfc\u5165\u6b63\u5728\u8fdb\u884c\uff0c\u8bf7\u7a0d\u540e\u3002"));
        return;
    }

    QStringList jars;
    for (const QString& p : filePaths) {
        if (QFileInfo(p).suffix().compare(QLatin1String("jar"), Qt::CaseInsensitive) == 0) {
            jars << p;
        }
    }

    bool jarsToPointers = false;
    if (!jars.isEmpty()) {
        const QString destDesc = targetRel.isEmpty()
            ? QString::fromUtf8("\u5206\u652f\u6839\u76ee\u5f55")
            : QString::fromUtf8("branches/%1/%2").arg(branch_).arg(targetRel);
        const auto choice = QMessageBox::question(this,
            QString::fromUtf8("\u5bfc\u5165 JAR"),
            QString::fromUtf8("%1 \u4e2a JAR \u5c06\u5bfc\u5165 %2\n\n\u662f\u5426\u8f6c\u4e3a Modrinth \u6307\u9488?")
                .arg(jars.size()).arg(destDesc),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        jarsToPointers = (choice == QMessageBox::Yes);
    }

    const QString root = branchDir();

    // 单次合并导入: 所有文件（含 jar）统一按落点 targetRel 路由，避免二次调用被 importRunning_ 丢弃
    const QVector<ImportJob> jobs = makeImportJobs(filePaths, targetRel);

    int conflicts = 0;
    for (const auto& job : jobs) {
        if (QFile::exists(root + QLatin1Char('/') + job.rel)) ++conflicts;
    }

    bool overwrite = false;
    if (conflicts > 0) {
        const auto choice = QMessageBox::question(this,
            QString::fromUtf8("\u8986\u76d6\u786e\u8ba4"),
            QString::fromUtf8("%1 \u4e2a\u76ee\u6807\u6587\u4ef6\u5df2\u5b58\u5728\uff0c\u662f\u5426\u8986\u76d6\uff1f")
                .arg(conflicts),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        overwrite = (choice == QMessageBox::Yes);
    }

    startImport(filePaths, targetRel, jarsToPointers, overwrite);
}

void ModpackContentIde::startImport(const QStringList& filePaths,
    const QString& targetRel, bool jarsToPointers, bool overwriteExisting)
{
    if (filePaths.isEmpty() || importRunning_.load()) return;

    struct ImportStats {
        std::atomic<int> done{0};
        std::atomic<int> failed{0};
    };
    QVector<ImportJob> jobs = makeImportJobs(filePaths, targetRel);
    if (jobs.isEmpty()) {
        emit logMessage(QString::fromUtf8("\u65e0\u53ef\u5bfc\u5165\u7684\u6587\u4ef6\u3002"));
        return;
    }

    importCancelToken_ = std::make_unique<NeoCore::CancelToken>();
    NeoCore::CancelToken* cancel = importCancelToken_.get();
    importRunning_.store(true);

    const QString bcDir = branchConfigDir_;
    const QString branchName = branch_;
    const int total = jobs.size();
    const QString root = branchDir();
    const QString repoDir = repoDir_;
    const QString trash = trashDir();
    const QStringList chain = chain_;

    auto* card = spawnWorkCard(
        QString::fromUtf8("\u5bfc\u5165\u4e2d (%1 \u4e2a\u6587\u4ef6)").arg(total), true);
    connect(card, &HiBerGUI::WorkCard::cancelRequested, this, [this]() {
        if (importCancelToken_) {
            importCancelToken_->request_cancel();
        }
    });

    QPointer<ModpackContentIde> guard = this;
    QPointer<HiBerGUI::WorkCard> cardGuard = card;
    auto stats = std::make_shared<ImportStats>();
    auto overwritten = std::make_shared<QVector<OverwriteItem>>();
    if (importThread_ && importThread_->joinable()) {
        importThread_->join();
    }
    importThread_ = std::make_unique<std::thread>(
        [guard, cardGuard, cancel, stats, jobs, bcDir, branchName, root, trash,
         chain, repoDir, total, jarsToPointers, overwriteExisting, overwritten]() {
            QStringList copied;

            for (const ImportJob& job : jobs) {
                if (cancel->is_cancelled()) break;

                // 所有文件统一按落点路由: dstAbs = root/rel (rel 由 targetRel 决定)
                const QString dstAbs = root + QLatin1Char('/') + job.rel;
                QDir().mkpath(QFileInfo(dstAbs).absolutePath());

                bool ok = false;
                if (QFile::exists(dstAbs) && overwriteExisting) {
                    OverwriteItem item;
                    item.rel = job.rel;
                    item.parentHas = fileExistsInParentChain(repoDir, chain, job.rel);
                    item.shaBefore = computeSha256(dstAbs);
                    item.trashAbs = trash + QLatin1Char('/') + job.rel;
                    QDir().mkpath(QFileInfo(item.trashAbs).absolutePath());
                    QFile::remove(item.trashAbs);
                    if (QFile::rename(dstAbs, item.trashAbs)) {
                        item.movedOld = true;
                    }
                    if (QFile::copy(job.src, dstAbs)) {
                        const QString sha = computeSha256(dstAbs);
                        if (sha.length() == 64) {
                            updateFileManifest(bcDir, branchName, job.rel, sha);
                        }
                        if (item.movedOld) {
                            overwritten->push_back(item);
                        }
                        copied << dstAbs;
                        ok = true;
                    } else {
                        if (item.movedOld) {
                            QFile::rename(item.trashAbs, dstAbs);
                        }
                    }
                } else if (!QFile::exists(dstAbs)) {
                    if (QFile::copy(job.src, dstAbs)) {
                        const QString sha = computeSha256(dstAbs);
                        if (sha.length() == 64) {
                            updateFileManifest(bcDir, branchName, job.rel, sha);
                        }
                        copied << dstAbs;
                        ok = true;
                    }
                } else {
                    ok = true;
                }
                stats->done.fetch_add(1);
                if (!ok) stats->failed.fetch_add(1);

                const QString curRel = job.rel;
                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(guard.data(),
                        [guard, cardGuard, stats, total, curRel]() {
                            if (guard.isNull() || cardGuard.isNull()) return;
                            cardGuard->setProgress(
                                stats->done.load() * 100 / qMax(1, total),
                                QString::fromUtf8("\u5df2\u5904\u7406 %1/%2 \u00b7 %3")
                                    .arg(stats->done.load()).arg(total).arg(curRel));
                        }, Qt::QueuedConnection);
                }
            }

            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(),
                    [guard, cardGuard, copied, stats, jarsToPointers, root, jobs, overwritten]() {
                        if (guard.isNull()) return;
                        if (cardGuard) {
                            cardGuard->complete(
                                QString::fromUtf8("\u5bfc\u5165\u5b8c\u6210\uff0c\u5931\u8d25 %1 \u9879\u3002")
                                    .arg(stats->failed.load()));
                            guard->removeWorkCard(cardGuard.data());
                        }
                        if (stats->failed.load() > 0) {
                            // 不中断错误: toast 提示 (位于卡片堆上方)
                            guard->showErrorToast(
                                QString::fromUtf8("\u5bfc\u5165\u90e8\u5206\u5931\u8d25"),
                                QString::fromUtf8("%1 \u9879\u672a\u5bfc\u5165\uff0c\u8bf7\u68c0\u67e5\u76ee\u6807\u76ee\u5f55\u6743\u9650\u6216\u5145\u4f59\u3002")
                                    .arg(stats->failed.load()));
                        }
                        guard->importRunning_.store(false);
                        if (guard->importThread_ && guard->importThread_->joinable()) {
                            guard->importThread_->join();
                        }
                        guard->importThread_.reset();
                        guard->importCancelToken_.reset();
                        if (!copied.isEmpty()) {
                            emit guard->gitAddRequested(copied);
                            emit guard->branchConfigChanged(guard->branch_);
                        }
                        guard->pushImportUndo(copied, overwritten, jobs);
                        guard->refreshBranchMeta();
                        // 拖入/导入后重新扫描磁盘文件树
                        if (guard->repoPanel_) {
                            guard->repoPanel_->refresh();
                        }
                        guard->refreshPreview();
                        if (jarsToPointers) {
                            // 落点原位转换: 仅转换本次实际复制成功的 jar
                            QStringList jarRels;
                            for (const ImportJob& job : jobs) {
                                const bool isJar = QFileInfo(job.src).suffix().compare(
                                    QLatin1String("jar"), Qt::CaseInsensitive) == 0;
                                if (isJar && copied.contains(root
                                        + QLatin1Char('/') + job.rel)) {
                                    jarRels << job.rel;
                                }
                            }
                            if (!jarRels.isEmpty()) {
                                guard->pointerEditor_->setContext(
                                    guard->repoDir_, guard->branch_,
                                    guard->branchConfigDir_);
                                guard->pointerEditor_->batchConvertJarsList(jarRels);
                            }
                        }
                    }, Qt::QueuedConnection);
            }
        });
}

void ModpackContentIde::onTreeObjectActivated(QWidget* panel,
    const RepoObjectInfo& info)
{
    QList<RepoObjectInfo> sel;
    if (auto* rp = qobject_cast<HiBerGUI::RepoTreePanel*>(panel)) {
        sel = rp->selectedObjects();
    } else if (auto* op = qobject_cast<HiBerGUI::OutputTreePanel*>(panel)) {
        sel = op->selectedObjects();
    }
    CLogger::Info("Tree activated: type={} path={} display={} inherited={} multi={}",
        static_cast<int>(info.type), info.path.toStdString(),
        info.displayName.toStdString(), info.isInherited, sel.size());
    if (info.type == RepoObjectType::Root) {
        // 根目录: 打开根目录文件夹策略编辑器 (folders[""] = 根策略)
        const PolicySnapshot snap = parsePolicies(repoDir_, branch_);
        const auto it = snap.folders.find(QString());
        const bool overrides = snap.branchFolderOverrides.count(QString()) > 0;
        const QString effective = (it != snap.folders.end())
            ? it->second : snap.defaultFolderPolicy;
        folderEditor_->load(QString(), effective, overrides, branch_);
        switchEditor(1);
        return;
    }
    if (info.path.isEmpty()) {
        switchEditor(0);
        return;
    }
    // 多选 (>1) 且单击节点在选集中 -> 打开批量编辑器; 否则单对象路由
    bool inSel = false;
    for (const RepoObjectInfo& s : sel) {
        if (s.path == info.path) {
            inSel = true;
            break;
        }
    }
    if (sel.size() > 1 && inSel) {
        batchEditor_->loadSelection(sel);
        switchEditor(6);
        return;
    }
    routeObject(info);
}

void ModpackContentIde::applyBatchPolicy(const QStringList& paths,
    const QString& mode, const std::vector<std::string>& trackedKeys,
    const std::vector<int>& trackedLines, bool toBranch)
{
    if (paths.isEmpty() || repoDir_.isEmpty() || branch_.isEmpty()) return;

    // 捕获受影响路径的旧策略, 供撤销恢复
    auto oldEntries = std::make_shared<QMap<QString, FilePolicy>>();
    const PolicySnapshot snap = parsePolicies(repoDir_, branch_);
    for (const QString& p : paths) {
        const auto it = snap.files.find(p);
        if (it != snap.files.end()) {
            oldEntries->insert(p, it->second);
        } else {
            oldEntries->insert(p, FilePolicy{});
        }
    }

    auto doFn = [this, paths, mode, trackedKeys, trackedLines, toBranch]() {
        emit batchPolicySaveRequested(paths, mode, trackedKeys, trackedLines,
            toBranch);
    };
    auto undoFn = [this, oldEntries, toBranch]() {
        for (auto it = oldEntries->begin(); it != oldEntries->end(); ++it) {
            std::vector<std::string> k;
            k.reserve(it->keys.size());
            for (const QString& s : it->keys) {
                k.push_back(s.toStdString());
            }
            std::vector<int> ln;
            ln.reserve(it->lines.size());
            for (int v : it->lines) {
                ln.push_back(v);
            }
            emit batchPolicySaveRequested({ it.key() }, it->mode, k, ln,
                toBranch);
        }
    };
    undoStack_->push(new BranchFileCommand(std::move(doFn), std::move(undoFn),
        QString::fromUtf8("\u6279\u91cf\u4fee\u6539\u540c\u6b65\u7b56\u7565 (%1 \u9879)")
            .arg(paths.size())));
    refreshFolderEditorState();
}

void ModpackContentIde::routeObject(const RepoObjectInfo& info)
{
    const PolicySnapshot snap = parsePolicies(repoDir_, branch_);
    // 用户标记为配置文件的路径集合 (workspace.json sync_policies.config_files)
    QSet<QString> markedConfigs;
    {
        QFile f(repoDir_ + QStringLiteral("/workspace.json"));
        if (f.open(QIODevice::ReadOnly)) {
            try {
                const auto j = nlohmann::json::parse(f.readAll().toStdString());
                f.close();
                auto collect = [&markedConfigs](const nlohmann::json& sp) {
                    if (!sp.is_object()) return;
                    if (sp.contains("config_files") && sp["config_files"].is_array()) {
                        for (const auto& x : sp["config_files"]) {
                            if (x.is_string()) {
                                markedConfigs.insert(QString::fromStdString(x.get<std::string>()));
                            }
                        }
                    }
                };
                if (j.contains("sync_policies")) {
                    collect(j["sync_policies"]);
                }
                if (j.contains("branches") && j["branches"].is_array()) {
                    for (const auto& b : j["branches"]) {
                        if (!b.is_object()) continue;
                        if (b.value("name", "") != branch_.toStdString()) continue;
                        if (b.contains("sync_policies")) {
                            collect(b["sync_policies"]);
                        }
                        break;
                    }
                }
            } catch (...) {
                f.close();
            }
        }
    }

    switch (info.type) {
    case RepoObjectType::Root: {
        // 根目录: 根策略 (folders[""]) 或默认文件夹策略
        const auto it = snap.folders.find(QString());
        const bool overrides = snap.branchFolderOverrides.count(QString()) > 0;
        const QString effective = (it != snap.folders.end())
            ? it->second : snap.defaultFolderPolicy;
        folderEditor_->load(QString(), effective, overrides, branch_);
        switchEditor(1);
        break;
    }
    case RepoObjectType::Folder: {
        if (info.path == QStringLiteral("save/[save]/serverconfig")) {
            serverConfigEditor_->setContext(repoDir_, branch_);
            serverConfigEditor_->load();
            switchEditor(4);
            break;
        }
        const auto it = snap.folders.find(info.path);
        const bool overrides = snap.branchFolderOverrides.count(info.path) > 0;
        const QString effective = (it != snap.folders.end())
            ? it->second : snap.defaultFolderPolicy;
        folderEditor_->load(info.path, effective, overrides, branch_);
        switchEditor(1);
        break;
    }
    case RepoObjectType::ConfigFile: {
        if (info.isInherited) {
            openContentEditor(info);
            break;
        }
        const auto it = snap.files.find(info.path);
        const bool overrides = snap.branchFileOverrides.count(info.path) > 0;
        FilePolicy fp;
        if (it != snap.files.end()) {
            fp = it->second;
        }
        const QString absPath = repoDir_ + QStringLiteral("/branches/")
            + branch_ + QLatin1Char('/') + info.path;
        // 输出树/继承场景: 实体可能在 .overrides 或父分支, 解析真实路径供解析器读取
        QString absResolved = absPath;
        if (!QFile::exists(absResolved)) {
            const QString ov = branchDir() + QStringLiteral("/.overrides/")
                + info.path;
            if (QFile::exists(ov)) {
                absResolved = ov;
            } else {
                const QString pe = parentEntityPath(info.path);
                if (!pe.isEmpty()) absResolved = pe;
            }
        }
        std::vector<std::string> keys;
        for (const auto& k : fp.keys) {
            keys.push_back(k.toStdString());
        }
        std::vector<int> lines;
        for (int l : fp.lines) {
            lines.push_back(l);
        }
        injectConfigEditorExt(info.path);
        configEditor_->load(info.path, absResolved, repoDir_, branch_, fp.mode,
            keys, lines, overrides);
        switchEditor(2);
        break;
    }
    case RepoObjectType::Pointer: {
        // 输出树指针项无 sha, 按 rel 反查
        const QString sha = info.pointerSha.isEmpty()
            ? pointerShaForRel(branchConfigDir_, branch_, info.path)
            : info.pointerSha;
        NeoCore::PointerFileData pfd;
        if (!sha.isEmpty() && readPointerFile(branchConfigDir_, sha, pfd)) {
            pointerEditor_->loadPointer(sha, pfd);
            switchEditor(3);
        } else {
            switchEditor(0);
        }
        break;
    }
    case RepoObjectType::PlainFile:
    default: {
        if (info.isInherited) {
            if (isTextEditableName(info.displayName)) {
                openContentEditor(info);
            } else {
                switchEditor(0);
            }
            break;
        }
        // 标记为配置文件的普通文件: 打开配置编辑器 (ConfigFileEditor)
        if (snap.files.count(info.path) > 0 || markedConfigs.contains(info.path)) {
            const auto it = snap.files.find(info.path);
            const bool overrides = snap.branchFileOverrides.count(info.path) > 0;
            FilePolicy fp = (it != snap.files.end()) ? it->second : FilePolicy();
            const QString absPath = repoDir_ + QStringLiteral("/branches/")
                + branch_ + QLatin1Char('/') + info.path;
            std::vector<std::string> keys;
            for (const auto& k : fp.keys) {
                keys.push_back(k.toStdString());
            }
            std::vector<int> lines;
            for (int l : fp.lines) {
                lines.push_back(l);
            }
            injectConfigEditorExt(info.path);
            configEditor_->load(info.path, absPath, repoDir_, branch_, fp.mode,
                keys, lines, overrides);
            switchEditor(2);
            break;
        }
        const QString absPath = repoDir_ + QStringLiteral("/branches/")
            + branch_ + QLatin1Char('/') + info.path;
        if (QFile::exists(absPath)) {
            // 文本可编辑文件 (.xml/.cfg/.conf/md/log 等) 打开内容编辑器, 其余文件才进入转指针
            if (isTextEditableName(info.displayName)) {
                openContentEditor(info);
            } else {
                pointerEditor_->loadFileToConvert(info.path, absPath);
                switchEditor(3);
            }
        } else {
            switchEditor(0);
        }
        break;
    }
    }
}

void ModpackContentIde::switchEditor(int index)
{
    if (index < 0 || index >= editorStack_->count()) return;
    editorStack_->setCurrentIndex(index);

    // 淡入动画: 合成层仅在动画期间挂载, 结束后立即移除——
    // QGraphicsEffect 常驻会导致动态内容 (解析器卡片/QScrollArea) 离屏缓存失效不及时
    // 注意: QWidget::setGraphicsEffect 拥有 effect 所有权——安装新 effect 或传 nullptr
    // 都会同步 delete 旧 effect, 严禁再手动 deleteLater (悬垂崩溃)
    auto* eff = new QGraphicsOpacityEffect(editorStack_);
    eff->setOpacity(0.0);
    editorStack_->setGraphicsEffect(eff);
    auto* fade = new QPropertyAnimation(eff, "opacity", this);
    fade->setDuration(160);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    QPointer<QStackedWidget> stackGuard = editorStack_;
    QPointer<QGraphicsOpacityEffect> effGuard = eff;
    QObject::connect(fade, &QPropertyAnimation::finished, editorStack_,
        [stackGuard, effGuard]() {
            if (stackGuard && effGuard) {
                // 仅当自己仍是当前 effect 时移除 (同步删除, 恢复直接渲染);
                // 快速连续切换时旧 effect 已被自动删除, effGuard 为空 → 不误删新 effect
                stackGuard->setGraphicsEffect(nullptr);
            }
        });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

HiBerGUI::WorkCard* ModpackContentIde::spawnWorkCard(const QString& title,
    bool cancelable)
{
    return workStack_ ? workStack_->addCard(title, cancelable) : nullptr;
}

void ModpackContentIde::removeWorkCard(HiBerGUI::WorkCard* card)
{
    if (workStack_) {
        workStack_->removeCard(card);
    }
}

void ModpackContentIde::showErrorToast(const QString& title, const QString& detail)
{
    if (!toast_) return;
    // toast 显示于卡片堆上方: 偏移 = 卡片堆折叠高度 (若展开则按展开高度)
    const int stackH = workStack_ && workStack_->isVisible()
        ? workStack_->height() : 0;
    toast_->setTopOffset(stackH + 8);
    toast_->showError(title, detail, 4000);
}

void ModpackContentIde::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // 卡片堆自锚定 (WorkCardStack 监听宿主 Resize 自动 reposition)
}

} // namespace GUIWorker

#include "modpack_content_ide.moc"
