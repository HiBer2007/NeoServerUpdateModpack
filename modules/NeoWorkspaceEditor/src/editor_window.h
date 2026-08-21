#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QComboBox>
#include <QLabel>
#include <QSettings>
#include <QFileInfo>
#include <QStringList>
#include <QSplitter>
#include <QResizeEvent>

#include <string>
#include <set>
#include <map>

#include <nlohmann/json.hpp>

#include "progress_card.h"

namespace GUIWorker {
class RepoEditor;
class BranchEditor;
class ModpackContentIde;
}

namespace HiBerGUI {
class GitPanel;
}

enum class RepoSource { Local, RemoteCache, Clone, New };

class EditorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorWindow(QWidget* parent = nullptr);
    ~EditorWindow();

    bool loadWorkspace(const std::string& dirPath,
        RepoSource source = RepoSource::Local);
    bool saveWorkspace();
    bool saveWorkspaceAs();
    bool isModified() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onVerify();
    void onExportBackup();
    void onNewRepo();
    void onCloneRepo();
    void onSshKeys();
    void onBranchChanged(const std::string& branchName);
    void onAnyModified();
    void onGitInfo();
    void onCreateBranch();
    void onSwitchBranch();
    void onForkRepo();
    void onBranchMeta();

private:
    void buildUI();
    void clampGitPanelWidth();
    void buildMenus();
    void rebuildExtensionsMenu();
    void buildToolBar();
    void connectSignals();
    void updateTitle();
    void updateStatus();
    void updateBranchSelector();
    bool maybeSave();
    void ensureBranchConfigs();
    nlohmann::json branchConfig(const std::string& name);
    void saveBranchConfig(const std::string& name);
    std::string branchConfigDir() const;
    std::pair<std::string, RepoSource> selectWorkspaceFile();

    void onCheckIntegrity();
    void runIntegrityCheck(bool manual);
    // 完整性检查: git status 异步完成后在 GUI 线程解析并展示结果
    void finishIntegrityCheck(const QString& statusOut, const QString& dir,
        bool manual);
    void gitAddPaths(const QStringList& paths);
    void ensureGitIgnore(const QString& dir);

    void loadCustomLayout();
    void saveCustomLayout();

    QTabWidget* tabWidget_;

    GUIWorker::RepoEditor* repoEditor_;
    GUIWorker::BranchEditor* branchEditor_;
    GUIWorker::ModpackContentIde* contentIde_;

    QToolBar* toolBar_;
    QAction* openAction_;
    QAction* saveAction_;
    QAction* saveAsAction_;
    QAction* verifyAction_;
    QAction* backupAction_;
    QAction* newRepoAction_;
    QAction* cloneRepoAction_;
    QAction* sshKeysAction_;
    QAction* integrityAction_;
    QAction* gitInfoAction_;
    QAction* createBranchAction_;
    QAction* switchBranchAction_;
    QAction* forkRepoAction_;
    QAction* branchMetaAction_;
    QComboBox* branchCombo_;
    QMenu* extMenu_ = nullptr;

    QLabel* filePathLabel_;
    QLabel* modifiedLabel_;
    QLabel* gitStatusLabel_;

    HiBerGUI::GitPanel* gitPanel_;
    QSplitter* mainSplitter_;

    HiBerGUI::ProgressCard* loadProgressCard_ = nullptr;
    void positionLoadCard();
    // 加载进度卡片阶段更新 (loadWorkspace 与异步完整性检查共用)
    void updateLoadProgress(int pct, const QString& text);

    std::string currentFilePath_;
    nlohmann::json workspaceConfig_;
    std::map<std::string, nlohmann::json> branchConfigs_;
    bool modified_;

    // 完整性检查异步进行中标志 (防重入)
    bool integrityBusy_ = false;

    QSettings settings_;
    QStringList recentFiles_;
    static const int MaxRecentFiles = 10;

    static const char* ConfigVersion;
};
