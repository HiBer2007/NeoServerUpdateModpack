#include "modpack_content_ide.h"

#include "folder_policy_editor.h"
#include "config_file_editor.h"
#include "pointer_editor_panel.h"
#include "serverconfig_rules_editor.h"
#include "sync_policies_editor.h"
#include "file_content_editor.h"

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
#include <QUndoStack>
#include <QUndoCommand>
#include <QSet>

#include <map>
#include <set>
#include <functional>

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

class BranchFileCommand : public QUndoCommand {
public:
    BranchFileCommand(std::function<void()> doFn, std::function<void()> undoFn,
        const QString& text)
        : QUndoCommand(text)
        , doFn_(std::move(doFn))
        , undoFn_(std::move(undoFn))
    {
    }

    void redo() override
    {
        if (doFn_) doFn_();
    }

    void undo() override
    {
        if (undoFn_) undoFn_();
    }

private:
    std::function<void()> doFn_;
    std::function<void()> undoFn_;
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

struct ImportJob {
    QString src;
    QString rel;
};

QVector<ImportJob> makeImportJobs(const QStringList& filePaths,
    const QString& targetRel)
{
    QVector<ImportJob> jobs;
    auto appendFile = [&jobs](const QString& src, const QString& relDir) {
        const QString name = QFileInfo(src).fileName();
        const bool isJar = QFileInfo(src).suffix().compare(
            QLatin1String("jar"), Qt::CaseInsensitive) == 0;
        QString rel;
        if (isJar) {
            rel = QStringLiteral("mods/") + name;
        } else if (relDir.isEmpty()) {
            rel = name;
        } else {
            rel = relDir + QLatin1Char('/') + name;
        }
        jobs.push_back({ src, rel });
    };
    for (const QString& p : filePaths) {
        if (QFileInfo(p).isDir()) {
            QDirIterator it(p, QDir::Files | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString f = it.next();
                const QString sub = QDir(p).relativeFilePath(f);
                QString relDir = targetRel;
                if (!relDir.isEmpty()) {
                    relDir += QLatin1Char('/');
                }
                relDir += QFileInfo(sub).path();
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
    buildUI();
}

ModpackContentIde::~ModpackContentIde()
{
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

    editorEffect_ = new QGraphicsOpacityEffect(editorStack_);
    editorStack_->setGraphicsEffect(editorEffect_);
    editorEffect_->setOpacity(1.0);
    editorFade_ = new QPropertyAnimation(editorEffect_, "opacity", this);
    editorFade_->setDuration(160);

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

    progressCard_ = new ProgressCard(this);
    progressCard_->hideCard();

    buildOptionsPanel();
    CLogger::Info("BISECT heap after buildOptionsPanel: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    connect(outputPanel_, &OutputTreePanel::formatChanged, this,
        [this](const QString&) { refreshPreview(); });
    connect(outputPanel_, &OutputTreePanel::refreshRequested, this,
        &ModpackContentIde::refreshPreview);
    connect(outputPanel_, &OutputTreePanel::filesDropped, this,
        &ModpackContentIde::importDroppedFiles);
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
    connect(outputPanel_, &OutputTreePanel::newFolderRequested, this,
        &ModpackContentIde::createBranchFolder);
    connect(repoPanel_, &RepoTreePanel::objectActivated, this,
        &ModpackContentIde::routeObject);
    connect(repoPanel_, &RepoTreePanel::filesDropped, this,
        &ModpackContentIde::importDroppedFiles);
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
        &ModpackContentIde::folderPolicySaveRequested);
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
            undoStack_->push(new BranchFileCommand(
                [this, converted]() {
                    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
                    pointerEditor_->redoBatchConvert(*converted);
                },
                [this, converted]() {
                    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
                    pointerEditor_->undoBatchConvert(*converted);
                },
                QString::fromUtf8("\u6279\u91cf\u8f6c\u6362 JAR\u2192\u6307\u9488")));
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
    connect(progressCard_, &ProgressCard::cancelRequested, this, [this]() {
        if (importRunning_.load()) {
            if (importCancelToken_) {
                importCancelToken_->request_cancel();
            }
        } else if (cancelToken_) {
            cancelToken_->request_cancel();
        }
    });
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
    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);

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
    repoPanel_->setInheritedFiles(collectInheritedFiles(repoDir_, chain_, dir));
    repoPanel_->setBranchManifest(readBranchManifest(dir));
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

    QStringList files;
    if (isDir) {
        const QString base = branchDir() + QLatin1Char('/') + rel;
        QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            files << QDir(base).relativeFilePath(it.filePath());
        }
        if (files.isEmpty()) return;
    } else {
        files << rel;
    }
    deleteFileList(files);
}

void ModpackContentIde::deletePaths(const QList<RepoObjectInfo>& infos)
{
    if (infos.isEmpty()) return;
    QStringList files;
    for (const RepoObjectInfo& info : infos) {
        if (info.type == RepoObjectType::Folder) {
            const QString base = branchDir() + QLatin1Char('/') + info.path;
            QDirIterator it(base, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                files << QDir(base).relativeFilePath(it.filePath());
            }
        } else {
            files << info.path;
        }
    }
    files.removeDuplicates();
    if (files.isEmpty()) return;
    deleteFileList(files);
}

void ModpackContentIde::deleteFileList(const QStringList& files)
{
    if (files.isEmpty()) return;

    auto state = std::make_shared<DeleteState>();
    const QString repoDir = repoDir_;
    const QString branchDir = this->branchDir();
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;
    const QStringList chain = chain_;
    const QString trash = trashDir();

    auto doDelete = [state, repoDir, branchDir, bcDir, branch, chain, trash,
                     files, this]() {
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
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
        CLogger::Info("ModpackContentIde: deleted {} files (branch {})",
            files.size(), branch.toStdString());
    };

    auto undoDelete = [state, repoDir, branchDir, bcDir, branch, chain, trash,
                       files, this]() {
        QStringList changed;
        for (int i = files.size() - 1; i >= 0; --i) {
            const QString& f = files[i];
            const QString trashAbs = trash + QLatin1Char('/') + f;
            if (state->movedSha.contains(f)) {
                const QString abs = branchDir + QLatin1Char('/') + f;
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

    undoStack_->push(new BranchFileCommand(std::move(doDelete),
        std::move(undoDelete),
        QString::fromUtf8("\u5220\u9664 %1 \u4e2a\u6587\u4ef6").arg(files.size())));
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

    auto doPaste = [ops, branchDir, this]() {
        QStringList changed;
        for (const PasteOp& op : *ops) {
            QDir().mkpath(QFileInfo(op.dst).absolutePath());
            QFile::remove(op.dst);
            bool ok = false;
            if (op.wasCut) {
                ok = QFile::rename(op.src, op.dst);
            } else {
                ok = QFile::copy(op.src, op.dst);
            }
            if (ok) {
                changed << op.dst;
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

    undoStack_->push(new BranchFileCommand(std::move(doPaste),
        std::move(undoPaste),
        isCut ? QString::fromUtf8("\u526a\u5207\u7c98\u8d34 %1 \u4e2a\u9879\u76ee")
              : QString::fromUtf8("\u590d\u5236\u7c98\u8d34 %1 \u4e2a\u9879\u76ee")
                    .arg(ops->size())));
    refreshBranchMeta();
    refreshPreview();
}

void ModpackContentIde::batchRestorePointers(const QList<RepoObjectInfo>& infos)
{
    if (repoDir_.isEmpty() || branch_.isEmpty()) return;
    QStringList shas;
    for (const RepoObjectInfo& info : infos) {
        if (info.type == RepoObjectType::Pointer && !info.pointerSha.isEmpty()) {
            shas << info.pointerSha;
        }
    }
    if (shas.isEmpty()) return;

    pointerEditor_->setContext(repoDir_, branch_, branchConfigDir_);
    int okCount = 0;
    for (const QString& sha : shas) {
        NeoCore::PointerFileData pfd;
        if (!readPointerFile(branchConfigDir_, sha, pfd)) continue;
        if (pfd.resolvers.empty() || pfd.original_names.empty()) continue;
        if (pointerEditor_->restorePointerFromCache(sha, pfd)) {
            ++okCount;
        }
    }
    emit logMessage(QString::fromUtf8("\u6279\u91cf\u8f6c\u56de\u5b8c\u6210: %1 / %2")
        .arg(okCount).arg(shas.size()));
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

void ModpackContentIde::pushOverwriteUndo(
    const std::shared_ptr<QVector<OverwriteItem>>& items)
{
    if (!items || items->isEmpty()) return;
    const QString branchDir = this->branchDir();
    const QString bcDir = branchConfigDir_;
    const QString branch = branch_;

    auto doFn = [items, branchDir, this]() {
        QStringList changed;
        for (const auto& item : *items) {
            if (!item.parentHas) continue;
            auto markers = readBranchManifest(branchDir);
            markers[item.rel] = QStringLiteral("override");
            writeBranchManifest(branchDir, markers);
            changed << branchDir + QStringLiteral("/branch_manifest.json");
        }
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    auto undoFn = [items, branchDir, bcDir, branch, this]() {
        QStringList changed;
        for (int i = items->size() - 1; i >= 0; --i) {
            const OverwriteItem& item = (*items)[i];
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
        if (!changed.isEmpty()) {
            emit gitAddRequested(changed);
        }
    };

    undoStack_->push(new BranchFileCommand(std::move(doFn), std::move(undoFn),
        QString::fromUtf8("\u8986\u76d6\u5bfc\u5165 %1 \u4e2a\u6587\u4ef6")
            .arg(items->size())));
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

    progressCard_->showCard(
        QString::fromUtf8("\u751f\u6210\u9884\u89c8 (%1 / %2)").arg(branch_, format), true);
    positionProgressCard();
    previewRunning_.store(true);

    auto* self = this;
    previewThread_ = std::make_unique<std::thread>(
        [self, generation, layers = std::move(layers), targetDir, cancel]() {
        const bool compare = QDir(targetDir).exists();
        const std::string targetDirStd = compare ? targetDir.toStdString() : "";

        const nlohmann::json entries = NeoBuild::generateUmdStructureFromLayers(
            layers, targetDirStd, nullptr, cancel);
        const bool cancelled = cancel->is_cancelled();

        QMetaObject::invokeMethod(self, [self, generation, entries, cancelled]() {
            if (generation != self->previewGeneration_) {
                self->previewRunning_.store(false);
                return;
            }
            self->outputPanel_->loadEntries(entries);
            self->progressCard_->complete(cancelled
                ? QString::fromUtf8("\u5df2\u53d6\u6d88\uff0c\u663e\u793a\u90e8\u5206\u7ed3\u679c\u3002")
                : QString::fromUtf8("\u9884\u89c8\u751f\u6210\u5b8c\u6210\u3002"));
            QTimer::singleShot(600, self, [self]() {
                self->progressCard_->hideCard();
            });
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

    QStringList jars, others;
    for (const QString& p : filePaths) {
        if (QFileInfo(p).suffix().compare(QLatin1String("jar"), Qt::CaseInsensitive) == 0) {
            jars << p;
        } else {
            others << p;
        }
    }

    bool jarsToPointers = false;
    if (!jars.isEmpty()) {
        const auto choice = QMessageBox::question(this,
            QString::fromUtf8("\u5bfc\u5165 mods JAR"),
            QString::fromUtf8("%1 \u4e2a JAR \u5c06\u5bfc\u5165 branches/%2/mods/\n\n\u662f\u5426\u8f6c\u4e3a Modrinth \u6307\u9488?")
                .arg(jars.size()).arg(branch_),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        jarsToPointers = (choice == QMessageBox::Yes);
    }

    const QString root = branchDir();
    const QString modsDir = root + QStringLiteral("/mods");

    auto countConflicts = [root, modsDir, jarsToPointers](
        const QVector<ImportJob>& jobs) {
        int n = 0;
        for (const auto& job : jobs) {
            const QString dstAbs = jarsToPointers
                ? modsDir + QLatin1Char('/') + QFileInfo(job.rel).fileName()
                : root + QLatin1Char('/') + job.rel;
            if (QFile::exists(dstAbs)) ++n;
        }
        return n;
    };

    bool overwriteOthers = false;
    const QVector<ImportJob> othersJobs = makeImportJobs(others, targetRel);
    const int othersConflicts = countConflicts(othersJobs);
    if (othersConflicts > 0) {
        const auto choice = QMessageBox::question(this,
            QString::fromUtf8("\u8986\u76d6\u786e\u8ba4"),
            QString::fromUtf8("%1 \u4e2a\u76ee\u6807\u6587\u4ef6\u5df2\u5b58\u5728\uff0c\u662f\u5426\u8986\u76d6\uff1f")
                .arg(othersConflicts),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        overwriteOthers = (choice == QMessageBox::Yes);
    }

    bool overwriteJars = false;
    const QVector<ImportJob> jarsJobs = makeImportJobs(jars, QString());
    const int jarsConflicts = countConflicts(jarsJobs);
    if (jarsConflicts > 0) {
        const auto choice = QMessageBox::question(this,
            QString::fromUtf8("\u8986\u76d6\u786e\u8ba4"),
            QString::fromUtf8("%1 \u4e2a mods \u76ee\u6807\u5df2\u5b58\u5728\uff0c\u662f\u5426\u8986\u76d6\uff1f")
                .arg(jarsConflicts),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
            QMessageBox::No);
        if (choice == QMessageBox::Cancel) {
            return;
        }
        overwriteJars = (choice == QMessageBox::Yes);
    }

    startImport(others, targetRel, false, overwriteOthers);
    startImport(jars, QString(), jarsToPointers, overwriteJars);
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
    const QString modsDir = root + QStringLiteral("/mods");
    const QString trash = trashDir();
    const QStringList chain = chain_;

    progressCard_->showCard(
        QString::fromUtf8("\u5bfc\u5165\u4e2d (%1 \u4e2a\u6587\u4ef6)").arg(total), true);
    positionProgressCard();

    QPointer<ModpackContentIde> guard = this;
    auto stats = std::make_shared<ImportStats>();
    auto overwritten = std::make_shared<QVector<OverwriteItem>>();
    if (importThread_ && importThread_->joinable()) {
        importThread_->join();
    }
    importThread_ = std::make_unique<std::thread>(
        [guard, cancel, stats, jobs, bcDir, branchName, root, modsDir, trash,
         chain, repoDir, total, jarsToPointers, overwriteExisting, overwritten]() {
            QStringList copied;

            for (const ImportJob& job : jobs) {
                if (cancel->is_cancelled()) break;

                QString dstAbs;
                if (jarsToPointers) {
                    dstAbs = modsDir + QLatin1Char('/') + QFileInfo(job.rel).fileName();
                } else {
                    dstAbs = root + QLatin1Char('/') + job.rel;
                }
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

                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(guard.data(),
                        [guard, stats, total]() {
                            if (guard.isNull()) return;
                            guard->progressCard_->setProgress(
                                stats->done.load() * 100 / qMax(1, total),
                                QString::fromUtf8("\u5df2\u5904\u7406 %1/%2")
                                    .arg(stats->done.load()).arg(total));
                        }, Qt::QueuedConnection);
                }
            }

            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(),
                    [guard, copied, stats, jarsToPointers, modsDir, overwritten]() {
                        if (guard.isNull()) return;
                        guard->progressCard_->complete(
                            QString::fromUtf8("\u5bfc\u5165\u5b8c\u6210\uff0c\u5931\u8d25 %1 \u9879\u3002")
                                .arg(stats->failed.load()));
                        QTimer::singleShot(600, guard.data(), [guard]() {
                            if (guard.isNull()) return;
                            guard->progressCard_->hideCard();
                        });
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
                        guard->pushOverwriteUndo(overwritten);
                        guard->refreshBranchMeta();
                        guard->refreshPreview();
                        if (jarsToPointers) {
                            guard->pointerEditor_->setContext(
                                guard->repoDir_, guard->branch_,
                                guard->branchConfigDir_);
                            guard->pointerEditor_->batchConvertJars(modsDir);
                        }
                    }, Qt::QueuedConnection);
            }
        });
}

void ModpackContentIde::routeObject(const RepoObjectInfo& info)
{
    const PolicySnapshot snap = parsePolicies(repoDir_, branch_);

    switch (info.type) {
    case RepoObjectType::Root:
        switchEditor(0);
        break;
    case RepoObjectType::Folder: {
        if (info.path == QStringLiteral("[save]/serverconfig")) {
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
        std::vector<std::string> keys;
        for (const auto& k : fp.keys) {
            keys.push_back(k.toStdString());
        }
        std::vector<int> lines;
        for (int l : fp.lines) {
            lines.push_back(l);
        }
        configEditor_->load(info.path, absPath, repoDir_, branch_, fp.mode,
            keys, lines, overrides);
        switchEditor(2);
        break;
    }
    case RepoObjectType::Pointer: {
        NeoCore::PointerFileData pfd;
        if (readPointerFile(branchConfigDir_, info.pointerSha, pfd)) {
            pointerEditor_->loadPointer(info.pointerSha, pfd);
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
        const QString absPath = repoDir_ + QStringLiteral("/branches/")
            + branch_ + QLatin1Char('/') + info.path;
        if (QFile::exists(absPath)) {
            pointerEditor_->loadFileToConvert(info.path, absPath);
            switchEditor(3);
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

    editorFade_->stop();
    editorEffect_->setOpacity(0.0);
    editorStack_->setCurrentIndex(index);
    editorFade_->setStartValue(0.0);
    editorFade_->setEndValue(1.0);
    editorFade_->start();
}

void ModpackContentIde::positionProgressCard()
{
    if (progressCard_->isActive()) {
        progressCard_->move(width() - progressCard_->width() - 16, 16);
    }
}

void ModpackContentIde::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionProgressCard();
}

} // namespace GUIWorker

#include "modpack_content_ide.moc"
