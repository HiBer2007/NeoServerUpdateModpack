#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>
#include <QDateTime>

#include <atomic>
#include <memory>
#include <thread>
#include <functional>

#include <nlohmann/json.hpp>

#include "output_tree_panel.h"
#include "repo_tree_panel.h"
#include "progress_card.h"
#include "work_card.h"
#include "work_card_stack.h"
#include "toast_notification.h"
#include "file_content_editor.h"

#include <string>
#include <vector>

namespace NeoCore {
class CancelToken;
}

class QUndoStack;

class QTabWidget;

namespace HiBerGUI {
class OutputTreePanel;
class RepoTreePanel;
class ProgressCard;
class FileContentEditor;
struct RepoObjectInfo;
enum class RepoObjectType;
}

namespace GUIWorker {

struct OverwriteItem {
    QString rel;
    QString trashAbs;
    QString shaBefore;
    bool parentHas = false;
    bool movedOld = false;
};

struct ImportJob {
    QString src;
    QString rel;
};

class FolderPolicyEditor;
class ConfigFileEditor;
class PointerEditorPanel;
class ServerConfigRulesEditor;
class SyncPoliciesEditor;
class BatchEditorPanel;
class EditorExtensionRegistry;

using HiBerGUI::OutputTreePanel;
using HiBerGUI::RepoTreePanel;
using HiBerGUI::ProgressCard;
using HiBerGUI::FileContentEditor;
using HiBerGUI::RepoObjectInfo;
using HiBerGUI::RepoObjectType;

class ModpackContentIde : public QWidget {
    Q_OBJECT

public:
    explicit ModpackContentIde(QWidget* parent = nullptr);
    ~ModpackContentIde() override;

    void setRepository(const QString& repoDir);
    void setBranch(const QString& branch, const QString& branchConfigDir);
    QString currentBranch() const { return branch_; }
    HiBerGUI::OutputTreePanel* outputPanel() const { return outputPanel_; }
    HiBerGUI::RepoTreePanel* repoPanel() const { return repoPanel_; }
    EditorExtensionRegistry* extensionRegistry() const { return extRegistry_.get(); }

    // 异步文件操作 (耗时撤销/重做): 后台线程执行 + 右上角进度卡片; busy 期间拒绝并发
    // (非槽: std::function 参数不受 moc 支持)
    void runAsyncCommand(const QString& title,
        const std::function<void(const std::function<void(int, const QString&)>&)>& work,
        const std::function<void()>& onFinished = {});

public slots:
    void refreshPreview();
    void refreshPoliciesView();
    void refreshFileTree();
    // 从 workspace.json 收集 sync_policies.config_files 标记并注入两面板
    void reloadExtraConfigFiles();
    // 按文件路径扩展名注入配置编辑插件到 ConfigFileEditor (merge 预览键值对定位)
    void injectConfigEditorExt(const QString& relPath);
    void importDroppedFiles(const QStringList& filePaths, const QString& targetRel);
    void showDropTargetHint(const QString& targetRel, bool hovering);
    void applyFolderPolicyToSubfolders(const QString& folderPath,
        const QString& policy, bool toBranch);
    void refreshFolderEditorState();
    void undoBranchOp();
    void redoBranchOp();
    void deleteCurrentSelection();
    void restoreCurrentSelection();
    void onCommitFinished(const QString& branch);
    void cleanupOnExit();
    void rescanExtensions();

signals:
    void contentModified();
    void folderPolicySaveRequested(const QString& path, const QString& policy,
        bool toBranch);
    void filePolicySaveRequested(const QString& path, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void batchPolicySaveRequested(const QStringList& paths, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void topSyncPoliciesSaveRequested(const QString& jsonString);
    void branchConfigChanged(const QString& branch);
    void gitAddRequested(const QStringList& paths);
    void logMessage(const QString& line);
    void extensionRegistryChanged();
    // 普通文件 ↔ 配置文件标记变更 (宿主持久化到 workspace.json 并保存)
    void configFileMarkChanged(const QString& rel, bool mark);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    OutputTreePanel* outputPanel_;
    RepoTreePanel* repoPanel_;
    QWidget* optionsPanel_;
    QTabWidget* leftTabs_ = nullptr;
    QStackedWidget* editorStack_;
    FolderPolicyEditor* folderEditor_;
    ConfigFileEditor* configEditor_;
    PointerEditorPanel* pointerEditor_;
    ServerConfigRulesEditor* serverConfigEditor_;
    BatchEditorPanel* batchEditor_;

    // 右上角层叠工作卡片堆 (紧凑卡, 与仓库加载大卡分离) + 错误 toast (位于卡片上方)
    HiBerGUI::WorkCardStack* workStack_ = nullptr;
    HiBerGUI::ToastNotification* toast_ = nullptr;

    QString repoDir_;
    QString branch_;
    QString branchConfigDir_;

    std::atomic<bool> previewRunning_;
    std::unique_ptr<NeoCore::CancelToken> cancelToken_;
    std::unique_ptr<std::thread> previewThread_;
    int previewGeneration_ = 0;

    std::atomic<bool> importRunning_;
    std::unique_ptr<NeoCore::CancelToken> importCancelToken_;
    std::unique_ptr<std::thread> importThread_;

    QTimer* refreshTimer_ = nullptr;
    QDateTime fileTreeSnapshot_;

    // 项目选项顶层同步策略: 修改防抖自动保存 (代际号丢弃过期保存)
    int topSyncSaveGen_ = 0;

    void buildUI();
    void buildOptionsPanel();
    void routeObject(const RepoObjectInfo& info);
    void onTreeObjectActivated(QWidget* panel, const RepoObjectInfo& info);
    void applyBatchPolicy(const QStringList& paths, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void switchEditor(int index);
    // 层叠卡片堆: 生成一张工作卡 (右上角, 折叠堆叠/悬停展开), 完成时 removeWorkCard
    HiBerGUI::WorkCard* spawnWorkCard(const QString& title, bool cancelable);
    void removeWorkCard(HiBerGUI::WorkCard* card);
    // 不中断错误: toast 显示于卡片堆上方
    void showErrorToast(const QString& title, const QString& detail);
    QString branchDir() const;
    QStringList scanBranchRootDirs() const;
    QString trashDir() const;
    void refreshBranchMeta();
    QDateTime lastModifiedOf(const QString& dirPath) const;
    QWidget* activeLeftPanel() const;
    void createBranchFolder(const QString& parentRel);
    void createServerConfigFolder();
    void deletePath(const QString& rel, bool isDir);
    void deletePaths(const QList<RepoObjectInfo>& infos);
    void deleteFileList(const QStringList& files,
        const QStringList& dirsToRemove = {});
    void pasteBranchFiles(const QStringList& relPaths, const QString& targetRel,
        bool isCut);
    void batchRestorePointers(const QList<RepoObjectInfo>& infos);
    QStringList infosToPaths(const QList<RepoObjectInfo>& infos);
    void restoreInherited(const QString& rel);
    void pushImportUndo(const QStringList& copiedAbs,
        const std::shared_ptr<QVector<OverwriteItem>>& overwritten,
        const QVector<ImportJob>& jobs);
    void openContentEditor(const RepoObjectInfo& info);
    void onContentSave(const QString& relPath, const QString& content,
        bool inherited);
    QString parentEntityPath(const QString& rel) const;
    void startImport(const QStringList& filePaths, const QString& targetRel,
        bool jarsToPointers, bool overwriteExisting = false);

    QUndoStack* undoStack_;
    QStringList chain_;
    std::unique_ptr<EditorExtensionRegistry> extRegistry_;

    // 每项后台文件操作一个独立线程 (多卡并存)
    std::vector<std::unique_ptr<std::thread>> workThreads_;

    void scanExtensionDirs(EditorExtensionRegistry* reg);
};

} // namespace GUIWorker
