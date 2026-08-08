#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

#include <atomic>
#include <memory>
#include <thread>

#include <nlohmann/json.hpp>

#include "output_tree_panel.h"
#include "repo_tree_panel.h"
#include "progress_card.h"
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

class FolderPolicyEditor;
class ConfigFileEditor;
class PointerEditorPanel;
class ServerConfigRulesEditor;
class SyncPoliciesEditor;

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

public slots:
    void refreshPreview();
    void refreshPoliciesView();
    void importDroppedFiles(const QStringList& filePaths, const QString& targetRel);
    void undoBranchOp();
    void redoBranchOp();
    void deleteCurrentSelection();
    void restoreCurrentSelection();
    void onCommitFinished(const QString& branch);
    void cleanupOnExit();

signals:
    void contentModified();
    void folderPolicySaveRequested(const QString& path, const QString& policy,
        bool toBranch);
    void filePolicySaveRequested(const QString& path, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void topSyncPoliciesSaveRequested(const QString& jsonString);
    void branchConfigChanged(const QString& branch);
    void gitAddRequested(const QStringList& paths);
    void logMessage(const QString& line);

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
    ProgressCard* progressCard_;
    QGraphicsOpacityEffect* editorEffect_;
    QPropertyAnimation* editorFade_;

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

    void buildUI();
    void buildOptionsPanel();
    void routeObject(const RepoObjectInfo& info);
    void switchEditor(int index);
    void positionProgressCard();
    QString branchDir() const;
    QStringList scanBranchRootDirs() const;
    QString trashDir() const;
    void refreshBranchMeta();
    QWidget* activeLeftPanel() const;
    void createBranchFolder(const QString& parentRel);
    void deletePath(const QString& rel, bool isDir);
    void deletePaths(const QList<RepoObjectInfo>& infos);
    void deleteFileList(const QStringList& files);
    void pasteBranchFiles(const QStringList& relPaths, const QString& targetRel,
        bool isCut);
    void batchRestorePointers(const QList<RepoObjectInfo>& infos);
    QStringList infosToPaths(const QList<RepoObjectInfo>& infos);
    void restoreInherited(const QString& rel);
    void pushOverwriteUndo(const std::shared_ptr<QVector<OverwriteItem>>& items);
    void openContentEditor(const RepoObjectInfo& info);
    void onContentSave(const QString& relPath, const QString& content,
        bool inherited);
    QString parentEntityPath(const QString& rel) const;
    void startImport(const QStringList& filePaths, const QString& targetRel,
        bool jarsToPointers, bool overwriteExisting = false);

    QUndoStack* undoStack_;
    QStringList chain_;
};

} // namespace GUIWorker
