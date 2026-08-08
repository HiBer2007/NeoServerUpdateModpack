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

#include <string>
#include <set>
#include <map>

#include <nlohmann/json.hpp>

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
    void buildMenus();
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
    void gitAddPaths(const QStringList& paths);
    void ensureGitIgnore(const QString& dir);

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

    QLabel* filePathLabel_;
    QLabel* modifiedLabel_;
    QLabel* gitStatusLabel_;

    HiBerGUI::GitPanel* gitPanel_;
    QSplitter* mainSplitter_;

    std::string currentFilePath_;
    nlohmann::json workspaceConfig_;
    std::map<std::string, nlohmann::json> branchConfigs_;
    bool modified_;

    QSettings settings_;
    QStringList recentFiles_;
    static const int MaxRecentFiles = 10;

    static const char* ConfigVersion;
};
