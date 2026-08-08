#include "editor_window.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QTextStream>

#include <powerhelper_bridge.h>

#include <QCloseEvent>
#include <QFileInfo>
#include <QDateTime>
#include <QApplication>
#include <QFile>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <QInputDialog>
#include <QClipboard>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QTextEdit>
#include <QStandardPaths>
#include <QDir>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTableWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QAbstractItemView>
#include <iostream>
#include <stdexcept>
#include <intrin.h>
#include <crtdbg.h>

#include <git_operations.h>

#include <repo_editor.h>
#include <branch_editor.h>
#include <modpack_content_ide.h>
#include <logger.h>

#include "git_panel.h"
#include "branch_meta_dialog.h"

namespace {

static void triggerStackOverflow(volatile int depth)
{
    volatile char pad[1024];
    pad[0] = (char)(depth & 0xFF);
    if (depth > 0) triggerStackOverflow(depth - 1);
}

class CrashLabel : public QLabel {
public:
    using QLabel::QLabel;

    void installFilter(QWidget* window) {
        window->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->modifiers() & Qt::ControlModifier) {
                switch (ke->key()) {
                    case Qt::Key_N: heldKey_ = "nullptr";  break;
                    case Qt::Key_S: heldKey_ = "stack";    break;
                    case Qt::Key_D: heldKey_ = "div0";     break;
                    case Qt::Key_T: heldKey_ = "throw";    break;
                    case Qt::Key_B: heldKey_ = "bkpt";     break;
                    case Qt::Key_I: heldKey_ = "illegal";  break;
                    case Qt::Key_F: heldKey_ = "heap";     break;
                    case Qt::Key_U: heldKey_ = "purecall"; break;
                    case Qt::Key_G: heldKey_ = "guard";    break;
                    default: break;
                }
            }
        } else if (ev->type() == QEvent::KeyRelease) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (!(ke->modifiers() & Qt::ControlModifier))
                heldKey_.clear();
        }
        return QLabel::eventFilter(obj, ev);
    }

    void mousePressEvent(QMouseEvent* ev) override {
        if (!countingDown_ && (ev->modifiers() & Qt::ControlModifier) && !heldKey_.isEmpty()) {
            startCountdown(heldKey_);
        }
        QLabel::mousePressEvent(ev);
    }
    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (countingDown_) { cancel(); }
        QLabel::mouseReleaseEvent(ev);
    }

private:
    void startCountdown(const QString& type) {
        countingDown_ = true;
        count_ = 10;
        crashType_ = type;
        originalText_ = text();
        CLogger::Warn("CRASH TEST: {} countdown started", crashType_.toStdString());
        setStyleSheet("color: #e51400; font-size: 9px; padding-right: 6px;");
        setText(QString("[%1] \u91ca\u653e\u53d6\u6d88: %2 \u79d2\u540e\u5f15\u7206").arg(crashType_).arg(count_));
        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, &CrashLabel::tick);
        timer_->start(1000);
    }
    void tick() {
        --count_;
        if (count_ > 0) {
            std::cerr << "[CRASH TEST] " << count_ << "..." << std::endl;
            CLogger::Warn("CRASH TEST: {}...", count_);
            setText(QString("测试崩溃 [%1] - %2").arg(crashType_).arg(count_));
        } else {
            std::cerr << "[CRASH TEST] BOOM! (" << crashType_.toStdString() << ")" << std::endl;
            CLogger::Error("CRASH TEST: BOOM! ({})", crashType_.toStdString());
            timer_->stop();
            if (crashType_ == "stack")
                triggerStackOverflow(100000);
            else if (crashType_ == "div0")
                { volatile int a = 1, b = 0; volatile int c = a / b; (void)c; }
            else if (crashType_ == "throw")
                throw std::runtime_error("CRASH TEST: unhandled C++ exception");
            else if (crashType_ == "bkpt")
                { __debugbreak(); }
            else if (crashType_ == "illegal")
                { __ud2(); }
            else if (crashType_ == "heap")
                { auto* p = new int; delete p; delete p; }
            else if (crashType_ == "purecall")
                { _purecall(); }
            else if (crashType_ == "guard")
                { static int depth = 0; ++depth; volatile char big[32768]; big[0] = 1; if (depth < 10) big[0] = 2; }
            else
                { volatile int* p = nullptr; *p = 0; }
        }
    }
    void cancel() {
        countingDown_ = false;
        if (timer_) { timer_->stop(); }
        setStyleSheet("color: #999; font-size: 9px; padding-right: 6px;");
        setText(originalText_);
        CLogger::Info("CRASH TEST: cancelled");
    }
    QTimer* timer_ = nullptr;
    int count_ = 0;
    bool countingDown_ = false;
    QString originalText_;
    QString crashType_;
    QString heldKey_;
};

} // anonymous namespace

typedef NeoWorkspace::GitResult GitResult;

static void autoCommitAndPush(const QString& repoDir, const QString& remoteUrl,
    const QString& branch);

enum class GitProtocol { Local, SSH, HTTPS, HTTP, Unknown };

static GitProtocol detectProtocol(const QString& url)
{
    if (url.isEmpty()) return GitProtocol::Local;
    if (url.startsWith("git@") || url.startsWith("ssh://")) return GitProtocol::SSH;
    if (url.startsWith("https://")) return GitProtocol::HTTPS;
    if (url.startsWith("http://")) return GitProtocol::HTTP;
    return GitProtocol::Unknown;
}

static QString protocolLabel(GitProtocol p)
{
    switch (p) {
        case GitProtocol::SSH:    return "SSH";
        case GitProtocol::HTTPS:  return "HTTPS";
        case GitProtocol::HTTP:   return "HTTP";
        case GitProtocol::Local:  return "本地";
        default:                  return "未知";
    }
}

static QString repoNameFromUrl(const QString& url)
{
    QString name = url.section('/', -1);
    name.remove(".git", Qt::CaseInsensitive);
    if (name.isEmpty()) name = "repository";
    return name;
}

const char* EditorWindow::ConfigVersion = "1.0";

EditorWindow::EditorWindow(QWidget* parent)
    : QMainWindow(parent), modified_(false),
      settings_(QCoreApplication::applicationDirPath() + "/config/custom/editor.ini",
                QSettings::IniFormat)
{
    setWindowTitle("NSUM \u4ed3\u5e93\u7ba1\u7406\u5668");
    resize(960, 700);
    setMinimumSize(800, 550);

    buildUI();
    buildMenus();
    buildToolBar();
    connectSignals();

    recentFiles_ = settings_.value("recentFiles2").toStringList();

    restoreGeometry(settings_.value("windowGeometry").toByteArray());
    auto sizes = settings_.value("splitterSizes").toByteArray();
    if (!sizes.isEmpty())
        mainSplitter_->restoreState(sizes);
    else
        mainSplitter_->setSizes({75, 885});

    updateTitle();
    updateStatus();
}

EditorWindow::~EditorWindow()
{
    CLogger::Info("Destruct flow: EditorWindow destructor start, heap={}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    CLogger::Info("Destruct flow: EditorWindow destructor end");
}
void EditorWindow::buildUI()
{
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);

    gitPanel_ = new HiBerGUI::GitPanel(mainSplitter_);
    gitPanel_->setMaximumWidth(420);
    // GitPanel 与领域层使用同一 git 路径 (由 main 经 InstallConfig/SetDefaultGitPath 统一设置)
    gitPanel_->setGitPath(QString::fromStdString(
        NeoWorkspace::GitOperations::GetDefaultGitPath()));

    tabWidget_ = new QTabWidget(mainSplitter_);

    repoEditor_ = new GUIWorker::RepoEditor(tabWidget_);
    CLogger::Info("BISECT heap after RepoEditor: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    branchEditor_ = new GUIWorker::BranchEditor(tabWidget_);
    CLogger::Info("BISECT heap after BranchEditor: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    contentIde_ = new GUIWorker::ModpackContentIde(tabWidget_);
    CLogger::Info("BISECT heap after ModpackContentIde: {}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");

    tabWidget_->addTab(repoEditor_, "仓库设置");
    tabWidget_->addTab(branchEditor_, "分支管理");
    tabWidget_->addTab(contentIde_, "\u6574\u5408\u5305\u5185\u5bb9");

    mainSplitter_->addWidget(gitPanel_);
    mainSplitter_->addWidget(tabWidget_);
    mainSplitter_->setStretchFactor(0, 1);
    mainSplitter_->setStretchFactor(1, 3);
    mainSplitter_->setSizes({280, 680});

    setCentralWidget(mainSplitter_);

    filePathLabel_ = new QLabel("未打开文件", this);
    modifiedLabel_ = new QLabel("", this);
    gitStatusLabel_ = new QLabel("", this);

    auto* statusBar = this->statusBar();
    statusBar->addWidget(filePathLabel_, 2);
    statusBar->addWidget(gitStatusLabel_, 1);
    statusBar->addPermanentWidget(modifiedLabel_);

    auto* verLabel = new CrashLabel("NSUM Editor v1.0.0", this);
    verLabel->setStyleSheet("color: #999; font-size: 9px; padding-right: 6px;");
    verLabel->installFilter(this);
    statusBar->addPermanentWidget(verLabel);
}

void EditorWindow::buildMenus()
{
    auto* fileMenu = menuBar()->addMenu("文件(&F)");

    auto* exitAction = fileMenu->addAction("\u9000\u51fa(&X)");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    auto* repoMenu = menuBar()->addMenu("仓库(&R)");

    openAction_ = repoMenu->addAction("打开仓库(&O)...");
    openAction_->setShortcut(QKeySequence::Open);

    auto* recentMenu = repoMenu->addMenu("最近打开(&R)");
    connect(recentMenu, &QMenu::aboutToShow, [this, recentMenu]() {
        recentMenu->clear();
        for (const auto& repoDir : recentFiles_) {
            QString wsPath = repoDir + "/workspace.json";
            QString display = repoDir;
            if (QFile::exists(wsPath)) {
                QFile f(wsPath);
                if (f.open(QIODevice::ReadOnly)) {
                    try {
                        auto j = nlohmann::json::parse(f.readAll().toStdString());
                        std::string remote = j.value("git", nlohmann::json::object()).value("remote", "");
                        if (!remote.empty())
                            display = QString::fromStdString(remote) + "  |  " + repoDir;
                    } catch (...) {}
                    f.close();
                }
            }
            auto* action = recentMenu->addAction(display);
            connect(action, &QAction::triggered, [this, repoDir]() {
                loadWorkspace(repoDir.toStdString(), RepoSource::Local);
            });
        }
        if (recentFiles_.isEmpty()) {
            recentMenu->addAction("(\u6682\u65e0)")->setEnabled(false);
        }
        recentMenu->addSeparator();
        auto* clearAction = recentMenu->addAction("清除历史");
        connect(clearAction, &QAction::triggered, [this]() {
            recentFiles_.clear();
            settings_.setValue("recentFiles2", recentFiles_);
        });
    });

    repoMenu->addSeparator();
    newRepoAction_ = repoMenu->addAction("新建仓库(&N)...");
    newRepoAction_->setShortcut(QKeySequence("Ctrl+N"));
    connect(newRepoAction_, &QAction::triggered, this, &EditorWindow::onNewRepo);
    cloneRepoAction_ = repoMenu->addAction("克隆仓库(&C)...");
    connect(cloneRepoAction_, &QAction::triggered, this, &EditorWindow::onCloneRepo);
    repoMenu->addSeparator();
    sshKeysAction_ = repoMenu->addAction("SSH 密钥管理(&K)...");
    connect(sshKeysAction_, &QAction::triggered, this, &EditorWindow::onSshKeys);

    auto* gitMenu = menuBar()->addMenu("Git(&G)");

    gitInfoAction_ = gitMenu->addAction("Git 信息(&I)");
    gitInfoAction_->setEnabled(false);
    connect(gitInfoAction_, &QAction::triggered, this, &EditorWindow::onGitInfo);

    gitMenu->addSeparator();

    auto* branchOpsMenu = gitMenu->addMenu("分支操作(&B)");

    createBranchAction_ = branchOpsMenu->addAction("\u521b\u5efa\u65b0\u5206\u652f(&N)...");
    createBranchAction_->setEnabled(false);
    connect(createBranchAction_, &QAction::triggered, this, &EditorWindow::onCreateBranch);

    switchBranchAction_ = branchOpsMenu->addAction("\u5207\u6362\u5206\u652f(&S)...");
    switchBranchAction_->setEnabled(false);
    connect(switchBranchAction_, &QAction::triggered, this, &EditorWindow::onSwitchBranch);

    branchOpsMenu->addSeparator();

    forkRepoAction_ = branchOpsMenu->addAction("分叉 Git 仓库(&F)...");
    forkRepoAction_->setEnabled(false);
    connect(forkRepoAction_, &QAction::triggered, this, &EditorWindow::onForkRepo);

    branchOpsMenu->addSeparator();

    branchMetaAction_ = branchOpsMenu->addAction("\u5206\u652f\u5c5e\u6027\u914d\u7f6e(&P)...");
    branchMetaAction_->setEnabled(false);
    connect(branchMetaAction_, &QAction::triggered, this, &EditorWindow::onBranchMeta);

    auto* editMenu = menuBar()->addMenu("编辑(&E)");
    auto* undoAction = editMenu->addAction("撤销(&U)");
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this]() {
        if (contentIde_) {
            contentIde_->undoBranchOp();
        }
    });
    auto* redoAction = editMenu->addAction("重做(&R)");
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this]() {
        if (contentIde_) {
            contentIde_->redoBranchOp();
        }
    });
    editMenu->addSeparator();
    auto* deleteAction = editMenu->addAction("删除(&D)");
    deleteAction->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(deleteAction, &QAction::triggered, this, [this]() {
        if (contentIde_) {
            contentIde_->deleteCurrentSelection();
        }
    });
    auto* restoreAction = editMenu->addAction("\u8fd8\u539f\u539f\u59cb\u7248\u672c(&V)");
    connect(restoreAction, &QAction::triggered, this, [this]() {
        if (contentIde_) {
            contentIde_->restoreCurrentSelection();
        }
    });

    auto* toolsMenu = menuBar()->addMenu("工具(&T)");
    verifyAction_ = toolsMenu->addAction("验证配置(&V)");
    verifyAction_->setShortcut(QKeySequence("Ctrl+E"));
    connect(verifyAction_, &QAction::triggered, this, &EditorWindow::onVerify);

    integrityAction_ = toolsMenu->addAction("\u5b8c\u6574\u6027\u68c0\u67e5(&I)");
    integrityAction_->setShortcut(QKeySequence("Ctrl+I"));
    integrityAction_->setEnabled(false);
    connect(integrityAction_, &QAction::triggered, this, &EditorWindow::onCheckIntegrity);

    backupAction_ = toolsMenu->addAction("导出备份(&B)...");
    connect(backupAction_, &QAction::triggered, this, &EditorWindow::onExportBackup);

    auto* helpMenu = menuBar()->addMenu("帮助(&H)");
    auto* docsAction = helpMenu->addAction("帮助文档(&D)");
    connect(docsAction, &QAction::triggered, []() {
        const QString docs = PowerHelper::Bridge::defaultDocsDir();
        if (!PowerHelper::Bridge::launchReader(docs)) {
            QMessageBox::warning(nullptr, "无法打开帮助",
                "\u672a\u627e\u5230 PowerHelper.exe\uff0c\u65e0\u6cd5\u6253\u5f00\u5e2e\u52a9\u6587\u6863\u3002");
        }
    });
    auto* aboutAction = helpMenu->addAction("关于(&A)");
    connect(aboutAction, &QAction::triggered, [this]() {
        QMessageBox::about(this, "\u5173\u4e8e NSUM \u7f16\u8f91\u5668",
            "NSUM \u4ed3\u5e93\u7ba1\u7406\u5668 v1.0\n\n"
            "\u57fa\u4e8e NeoServerUpdateModpack \u7684 Git \u4ed3\u5e93\u7ba1\u7406\u5de5\u5177\u3002\n"
            "\u76f4\u63a5\u7f16\u8f91 workspace.json \u5e76\u63d0\u4ea4\u5230 Git \u4ed3\u5e93\u3002");
    });
}

void EditorWindow::buildToolBar()
{
    toolBar_ = addToolBar("主工具栏");
    toolBar_->setMovable(false);
    toolBar_->setIconSize(QSize(16, 16));

    openAction_->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));

    toolBar_->addAction(openAction_);
    toolBar_->addSeparator();

    toolBar_->addWidget(new QLabel(" 分支: ", this));
    branchCombo_ = new QComboBox(this);
    branchCombo_->setMinimumWidth(180);
    branchCombo_->setEnabled(false);
    toolBar_->addWidget(branchCombo_);
}

void EditorWindow::connectSignals()
{
    connect(openAction_, &QAction::triggered, this, &EditorWindow::onOpen);

    connect(repoEditor_, &GUIWorker::RepoEditor::saveRequested,
        [this](QString jsonStr) {
            auto config = nlohmann::json::parse(jsonStr.toStdString());
            auto& ws = workspaceConfig_;
            if (config.contains("workspace")) ws["workspace"] = config["workspace"];
            if (config.contains("git")) ws["git"] = config["git"];
            if (config.contains("custom_mods")) ws["custom_mods"] = config["custom_mods"];
            if (config.contains("sync_policies")) {
                if (config["sync_policies"].is_null()) {
                    ws.erase("sync_policies");
                } else {
                    ws["sync_policies"] = config["sync_policies"];
                }
            }
            ws.erase("sync_target");
            ws.erase("directory_mapping");
            if (!currentFilePath_.empty()) {
                saveWorkspace();
            }
        });

    connect(repoEditor_, &GUIWorker::RepoEditor::contentModified,
        this, &EditorWindow::onAnyModified);

    connect(repoEditor_, &GUIWorker::RepoEditor::connectionTestClicked,
        [this](QString url) {
            CLogger::Info("Test connection: {}", url.toStdString());
            QMessageBox::information(this, "测试连接",
                QString("\u6b63\u5728\u6d4b\u8bd5\u8fde\u63a5:\n%1\n\n\u6b64\u529f\u80fd\u9700\u8981 Git \u73af\u5883\u652f\u6301\u3002").arg(url));
        });

    connect(branchEditor_, &GUIWorker::BranchEditor::saveRequested,
        [this](QString jsonStr) {
            auto branches = nlohmann::json::parse(jsonStr.toStdString());
            workspaceConfig_["branches"] = branches;
            ensureBranchConfigs();
            updateBranchSelector();
            if (!currentFilePath_.empty()) {
                saveWorkspace();
            }
        });

    connect(branchEditor_, &GUIWorker::BranchEditor::contentModified,
        this, &EditorWindow::onAnyModified);

    connect(contentIde_, &GUIWorker::ModpackContentIde::topSyncPoliciesSaveRequested,
        [this](QString jsonStr) {
            auto j = nlohmann::json::parse(jsonStr.toStdString());
            if (j.is_null()) {
                workspaceConfig_.erase("sync_policies");
            } else {
                workspaceConfig_["sync_policies"] = std::move(j);
            }
            if (!currentFilePath_.empty()) {
                saveWorkspace();
            }
            contentIde_->refreshPoliciesView();
            contentIde_->refreshPreview();
        });

    connect(contentIde_, &GUIWorker::ModpackContentIde::folderPolicySaveRequested,
        [this](QString path, QString policy, bool toBranch) {
            nlohmann::json* target = &workspaceConfig_["sync_policies"];
            if (toBranch) {
                std::string bn = contentIde_->currentBranch().toStdString();
                nlohmann::json* branchObj = nullptr;
                for (auto& b : workspaceConfig_["branches"]) {
                    if (b.value("name", "") == bn) {
                        branchObj = &b;
                        break;
                    }
                }
                if (branchObj) {
                    target = &(*branchObj)["sync_policies"];
                }
            }
            if (!target->is_object()) {
                *target = nlohmann::json::object();
            }
            if (!(*target)["folders"].is_object()) {
                (*target)["folders"] = nlohmann::json::object();
            }
            if (policy.isEmpty()) {
                (*target)["folders"].erase(path.toStdString());
            } else {
                (*target)["folders"][path.toStdString()] = policy.toStdString();
            }
            if (!currentFilePath_.empty()) {
                saveWorkspace();
            }
            if (!toBranch) {
                contentIde_->refreshPoliciesView();
            }
            contentIde_->refreshPreview();
        });

    connect(contentIde_, &GUIWorker::ModpackContentIde::filePolicySaveRequested,
        [this](QString path, QString mode,
            std::vector<std::string> trackedKeys,
            std::vector<int> trackedLines, bool toBranch) {
            nlohmann::json* target = &workspaceConfig_["sync_policies"];
            if (toBranch) {
                std::string bn = contentIde_->currentBranch().toStdString();
                nlohmann::json* branchObj = nullptr;
                for (auto& b : workspaceConfig_["branches"]) {
                    if (b.value("name", "") == bn) {
                        branchObj = &b;
                        break;
                    }
                }
                if (branchObj) {
                    target = &(*branchObj)["sync_policies"];
                }
            }
            if (!target->is_object()) {
                *target = nlohmann::json::object();
            }
            nlohmann::json entry;
            entry["mode"] = mode.toStdString();
            nlohmann::json keys = nlohmann::json::array();
            for (const auto& k : trackedKeys) keys.push_back(k);
            nlohmann::json lines = nlohmann::json::array();
            for (int l : trackedLines) lines.push_back(l);
            if (!keys.empty()) entry["tracked_keys"] = keys;
            if (!lines.empty()) entry["tracked_lines"] = lines;
            (*target)["files"][path.toStdString()] = entry;
            if (!currentFilePath_.empty()) {
                saveWorkspace();
            }
            if (!toBranch) {
                contentIde_->refreshPoliciesView();
            }
            contentIde_->refreshPreview();
        });

    connect(contentIde_, &GUIWorker::ModpackContentIde::contentModified,
        this, &EditorWindow::onAnyModified);

    connect(contentIde_, &GUIWorker::ModpackContentIde::branchConfigChanged,
        this, [this](const QString& branch) {
            const std::string name = branch.toStdString();
            const std::string dir = branchConfigDir();
            const std::string filePath = dir + "/" + name + ".json";
            QDir().mkpath(QString::fromStdString(dir));
            nlohmann::json j;
            {
                std::ifstream f(filePath);
                if (f.is_open()) {
                    try { j = nlohmann::json::parse(f); } catch (...) {}
                }
            }
            branchConfigs_[name] = std::move(j);
            gitAddPaths({ QString::fromStdString(filePath) });
            onAnyModified();
        });

    connect(contentIde_, &GUIWorker::ModpackContentIde::gitAddRequested,
        this, &EditorWindow::gitAddPaths);

    connect(contentIde_, &GUIWorker::ModpackContentIde::logMessage,
        this, [this](const QString& line) {
            statusBar()->showMessage(line, 5000);
        });

    connect(branchCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [this](int idx) {
            if (idx >= 0) {
                std::string branchName = branchCombo_->itemText(idx).toStdString();
                onBranchChanged(branchName);
            }
        });

    connect(gitPanel_, &HiBerGUI::GitPanel::statusChanged,
        [this](const QString& branch, int changes) {
            if (branch.isEmpty())
                gitStatusLabel_->setText("");
            else
                gitStatusLabel_->setText(
                    QString("%1 | %2").arg(branch)
                        .arg(changes > 0
                            ? QString("%1 \u9879\u6539\u52a8").arg(changes)
                            : "干净"));
        });

    connect(gitPanel_, &HiBerGUI::GitPanel::commitFinished, contentIde_,
        &GUIWorker::ModpackContentIde::onCommitFinished);
}

bool EditorWindow::loadWorkspace(const std::string& dirPath, RepoSource source)
{
    QString dir = QString::fromStdString(dirPath);
    QDir qdir(dir);
    if (!qdir.exists()) {
        QMessageBox::critical(this, "打开失败",
            QString("\u76ee\u5f55\u4e0d\u662f\u4ed3\u5e93:\n%1").arg(dir));
        return false;
    }

    NeoWorkspace::GitOperations gitOps;
    bool isGitRepo = gitOps.isGitRepository(dirPath);
    QString wsPath = dir + "/workspace.json";
    bool hasWorkspace = QFile::exists(wsPath);

    if (!isGitRepo && !hasWorkspace) {
        QMessageBox::critical(this, "无效仓库",
            QString("该目录既不是 Git 仓库也不包含 workspace.json:\n%1").arg(dir));
        return false;
    }

    if (!isGitRepo && hasWorkspace) {
        auto reply = QMessageBox::question(this, "\u521d\u59cb\u5316\u4ed3\u5e93",
            QString("\u76ee\u5f55\u4e0b\u7684 workspace.json \u5df2\u4e0d\u5728\u521d\u59cb\u5316 Git \u4ed3\u5e93:\n%1\n\n"
                    "\u662f\u5426\u521d\u59cb\u5316 Git \u4ed3\u5e93\u5e76\u91cd\u65b0\u63d0\u4ea4\u914d\u7f6e?").arg(dir),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            auto result = gitOps.init(dirPath);
            if (result.exitCode != 0) {
                QMessageBox::critical(this, "\u521d\u59cb\u5316\u5931\u8d25",
                    QString::fromStdString(result.stderrOutput));
                return false;
            }
            ensureGitIgnore(dir);
            gitOps.addAll(dirPath);
            gitOps.commit(dirPath, "initial workspace commit");
        } else {
            return false;
        }
    }

    if (isGitRepo && !hasWorkspace) {
        if (source == RepoSource::Clone) {
            QMessageBox::critical(this, "无效仓库",
                QString("远程仓库不是一个有效的整合包仓库。\n\n"
                        "仓库地址: %1\n"
                        "工作目录: %2\n\n"
                        "检查仓库地址是否正确或仓库是否有效！")
                    .arg(QString::fromStdString(gitOps.isGitRepository(dirPath)
                        ? (gitOps.revParse(dirPath, "HEAD").exitCode == 0 ? "(有效)" : "(无效)")
                        : ""),
                        dir));
            CLogger::Error("Workspace: cloned repo lacks workspace.json at {}", dirPath);
            return false;
        }

        CLogger::Warn("Workspace: {} repo without workspace.json at {}",
            source == RepoSource::RemoteCache ? "cached" : "local", dirPath);

        if (source == RepoSource::New) {
            nlohmann::json ws;
            ws["workspace"]["name"] = QFileInfo(dir).fileName().toStdString();
            ws["workspace"]["minecraft"] = "1.21.4";
            ws["workspace"]["modloader"] = "fabric";
            ws["git"]["remote"] = "";
            ws["git"]["default_branch"] = "main";
            ws["branches"] = nlohmann::json::array();
            ws["file_manifest"] = nlohmann::json::object();
            std::ofstream f(wsPath.toStdString());
            f << ws.dump(2) << std::endl;
            f.close();
            hasWorkspace = true;
        }

        if (!hasWorkspace) {
            QProcess proc;
            auto runGit = [&](const QStringList& args, int timeoutMs = 15000) {
                proc.setWorkingDirectory(dir);
                proc.start("git", args);
                proc.waitForFinished(timeoutMs);
                QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
                QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
                return std::make_pair(out, err);
            };

            QStringList log;
            auto appendLog = [&](const QString& line) {
                log.append(line);
                CLogger::Info("Workspace sync: {}", line.toStdString());
            };

            QString currentBranch;
            bool foundWs = false;
            bool recoveryDone = false;

            auto tryRecoveryPass = [&](bool fromRemote) -> bool {
                QApplication::setOverrideCursor(Qt::WaitCursor);

                if (fromRemote) {
                    appendLog("RemoteCache: \u8fdc\u7a0b origin \u5f3a\u5236\u540c\u6b65...");
                    appendLog("  git fetch origin");
                    auto [fo, fe] = runGit({"fetch", "origin"}, 60000);
                    if (currentBranch.isEmpty())
                        currentBranch = "main";
                    appendLog(QString("  git reset --hard origin/%1").arg(currentBranch));
                    auto [ro, re] = runGit({"reset", "--hard",
                        "origin/" + currentBranch}, 30000);
                if (!re.isEmpty()) appendLog("  reset output: " + re);
                else appendLog("  reset OK");
                } else {
                    appendLog("git fetch origin");
                    auto [fo, fe] = runGit({"fetch", "origin"}, 60000);
                    if (!fe.isEmpty() && !fe.contains("fatal"))
                appendLog("  fetch output: " + fe);
                else appendLog("  fetch OK");

                appendLog("Fetching current branch");
                    auto [bo, be] = runGit({"rev-parse", "--abbrev-ref", "HEAD"});
                    currentBranch = bo;
                    if (currentBranch.isEmpty() || currentBranch == "HEAD") {
                        std::vector<QString> cand = {"main", "master", "trunk"};
                        auto [bro, bre] = runGit({"branch", "-r"});
                        if (!bro.isEmpty()) {
                            for (auto& line : bro.split('\n', Qt::SkipEmptyParts)) {
                                line = line.trimmed();
                                if (line.startsWith("origin/")) {
                                    line.remove(QRegularExpression("^origin/"));
                                    if (!line.contains("HEAD") && !line.isEmpty())
                                        cand.push_back(line);
                                }
                            }
                        }
                        currentBranch = "main";
                        for (auto& c : cand) {
                            auto [ok, ign] = runGit({"show-ref", "--verify", "refs/heads/" + c});
                            if (!ok.isEmpty()) { currentBranch = c; break; }
                            auto [ok2, ign2] = runGit({"show-ref", "--verify",
                                "refs/remotes/origin/" + c});
                            if (!ok2.isEmpty()) { currentBranch = c; break; }
                        }
                appendLog(QString("  no current branch, using: %1").arg(currentBranch));
                    } else {
                appendLog(QString("  current branch: %1").arg(currentBranch));
                    }

                    appendLog("git reset --hard HEAD (\u8fd8\u539f\u5230\u4e0a\u6b21\u63d0\u4ea4\u72b6\u6001)");
                    auto [ro, re] = runGit({"reset", "--hard", "HEAD"}, 30000);
                if (!re.isEmpty()) appendLog("  reset output: " + re);
                else appendLog("  reset OK");
                }

                bool ws = QFile::exists(wsPath);
                if (!ws) {
                appendLog("git checkout -f to recover");
                    auto [co, ce] = runGit({"checkout", "-f", currentBranch}, 30000);
                    if (!ce.isEmpty()) {
                        auto [so, se] = runGit({"switch", currentBranch}, 30000);
                        if (!se.isEmpty()) {
                            auto [sco, sce] = runGit(
                                {"switch", "-c", currentBranch, "origin/" + currentBranch}, 30000);
                            if (!sce.isEmpty())
                appendLog("  switch failed: " + sce);
                        }
                    } else {
                appendLog("  checkout -f OK");
                    }
                    ws = QFile::exists(wsPath);
                }

                if (!ws) {
                    appendLog("git show HEAD:workspace.json \u52fe\u51fa\u5386\u53f2\u7248\u672c");
                    auto [sho, she] = runGit({"show", "HEAD:workspace.json"}, 10000);
                    if (!sho.isEmpty() && sho.contains("\"workspace\"")) {
                appendLog("  fetch OK");
                        std::ofstream f(wsPath.toStdString());
                        f << sho.toStdString() << std::endl;
                        f.close();
                        runGit({"add", "workspace.json"});
                        ws = true;
                    }
                }

                QApplication::restoreOverrideCursor();
                return ws;
            };

            auto askRecovery = [&]() -> bool {
                auto reply = QMessageBox::question(this, "\u7f3a\u5c11\u5de5\u4f5c\u533a\u914d\u7f6e",
                    QString("\u8be5\u76ee\u5f55\u662f Git \u4ed3\u5e93\u4f46\u7f3a\u5c11 workspace.json:\n%1\n\n"
                            "\u662f\u5426\u4ece Git \u4ed3\u5e93\u8fd8\u539f\u5de5\u4f5c\u533a\u914d\u7f6e?\n"
                            "(\u5c06\u6267\u884c: git fetch \u4e0e git reset --hard HEAD \u5e76\u9a8c\u8bc1)").arg(dir),
                    QMessageBox::Yes | QMessageBox::No);
                return reply == QMessageBox::Yes;
            };

            auto verifyWorkspace = [&]() -> bool {
                auto [so, se] = runGit({"status", "--porcelain"});
                bool clean = so.isEmpty();
                if (!clean) {
                    appendLog(QString("\u6e05\u7406: \u5c06\u4ece\u5de5\u4f5c\u533a\u5220\u9664\u7684 %1 \u4e2a\u9879\u76ee\u5217\u8868").arg(so.count('\n') + 1));
                } else {
            appendLog("repo self-check passed");
                }
                return clean;
            };

            auto findLastValidCommit = [&]() -> bool {
                appendLog("\u5bfb\u627e\u6700\u8fd1\u4e00\u4e2a workspace.json \u6709\u6548\u7684\u7248\u672c..");
                auto [lo, le] = runGit({"log", "--oneline", "-30", "--", "workspace.json"}, 10000);
                if (lo.isEmpty()) {
                    appendLog("  \u672a\u627e\u5230\u542b workspace.json \u7684\u7248\u672c");
                    return false;
                }

                QStringList commits;
                for (auto& line : lo.split('\n', Qt::SkipEmptyParts)) {
                    QString hash = line.section(' ', 0, 0).trimmed();
                    if (hash.length() >= 7) commits.append(hash);
                }
                if (commits.isEmpty()) return false;

                appendLog(QString("  \u627e\u5230 %1 \u4e2a\u53ef\u9009\u62e9\u7248\u672c").arg(commits.size()));

                for (auto& hash : commits) {
                    auto [sho, she] = runGit({"show", hash + ":workspace.json"}, 10000);
                    if (!sho.isEmpty() && sho.contains("\"workspace\"")) {
                        appendLog(QString("  \u5c1d\u8bd5 %1 \u7684 workspace.json").arg(hash.left(7)));
                        auto reply = QMessageBox::question(this, "找到有效提交",
                            QString("\u5728 %1 (%2) \u627e\u5230\u6709\u6548 workspace.json\u3002\n\n"
                                    "是否重置到该提交?")
                                .arg(hash.left(7),
                                     lo.split('\n', Qt::SkipEmptyParts).value(0)),
                            QMessageBox::Yes | QMessageBox::No);
                        if (reply == QMessageBox::Yes) {
                            QApplication::setOverrideCursor(Qt::WaitCursor);
                            auto [ro, re] = runGit({"reset", "--hard", hash}, 30000);
                            QApplication::restoreOverrideCursor();
                            if (!re.isEmpty()) {
                appendLog("  restore failed: " + re);
                            } else {
                appendLog(QString("  restoring commit %1").arg(hash.left(7)));
                                return QFile::exists(wsPath);
                            }
                        }
                        appendLog("  \u7528\u6237\u53d6\u6d88\u9009\u62e9\u3002");
                    }
                }
                return false;
            };

            if (!askRecovery()) return false;

            recoveryDone = tryRecoveryPass(false);
            foundWs = recoveryDone;

            if (!foundWs && source == RepoSource::RemoteCache) {
                QString logSoFar = log.join("\n");
                auto replay = QMessageBox::question(this, "本地恢复失败",
                    QString("\u4ece Git \u5386\u53f2\u6062\u590d\u5931\u8d25\u3002\n\n"
                            "操作日志:\n%1\n\n"
                            "\u662f\u5426\u4ece\u8fdc\u7a0b origin \u91cd\u65b0\u83b7\u53d6?").arg(logSoFar),
                    QMessageBox::Yes | QMessageBox::No);
                if (replay == QMessageBox::Yes) {
                    foundWs = tryRecoveryPass(true);
                }
            }

            if (foundWs && !verifyWorkspace()) {
                QString logSoFar = log.join("\n");
                auto vreply = QMessageBox::question(this, "\u81ea\u52a8\u6062\u590d\u5931\u8d25",
                    QString("工作区已恢复但存在未提交的更改，可能不完整。\n\n"
                            "\u662f\u5426\u5bfb\u627e\u6700\u8fd1\u4e00\u4e2a workspace.json \u6709\u6548\u7684\u7248\u672c?")
                        .arg(logSoFar),
                    QMessageBox::Yes | QMessageBox::No);
                if (vreply == QMessageBox::Yes) {
                    foundWs = findLastValidCommit();
                    if (foundWs) verifyWorkspace();
                }
            }

            if (!foundWs) {
                QString logDetail = log.join("\n");
                auto reply2 = QMessageBox::question(this, "恢复失败",
                    QString("未能自动恢复 workspace.json。\n\n"
                            "操作日志:\n%1\n\n"
                            "是否创建默认 workspace.json?").arg(logDetail),
                    QMessageBox::Yes | QMessageBox::No);
                if (reply2 == QMessageBox::Yes) {
                    nlohmann::json ws;
                    ws["workspace"]["name"] = QFileInfo(dir).fileName().toStdString();
                    ws["workspace"]["minecraft"] = "1.21.4";
                    ws["workspace"]["modloader"] = "fabric";
                    ws["git"]["remote"] = "";
                    ws["git"]["default_branch"] = "main";
                    ws["branches"] = nlohmann::json::array();
                    ws["file_manifest"] = nlohmann::json::object();
                    std::ofstream f(wsPath.toStdString());
                    f << ws.dump(2) << std::endl;
                    f.close();
                    foundWs = true;
                } else {
                    return false;
                }
            }
        }
    }

    std::ifstream file(wsPath.toStdString());
    if (!file.is_open()) {
        QMessageBox::critical(this, "打开失败", "无法读取 workspace.json");
        return false;
    }

    try {
        workspaceConfig_ = nlohmann::json::parse(file);
        file.close();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "解析失败",
            QString("workspace.json 解析错误:\n%1").arg(e.what()));
        return false;
    }

    currentFilePath_ = wsPath.toStdString();

    if (workspaceConfig_.contains("git") && workspaceConfig_["git"].contains("current_branch")) {
        auto target = workspaceConfig_["git"]["current_branch"].get<std::string>();
        NeoWorkspace::GitOperations git;
        auto cur = git.currentBranch(dir.toStdString());
        QString head = QString::fromStdString(cur.stdoutOutput).trimmed();
        if (!head.isEmpty() && head != "HEAD" && head.toStdString() != target) {
            auto result = git.checkout(dir.toStdString(), target);
            if (result.exitCode == 0) {
                std::ifstream file2(wsPath.toStdString());
                if (file2.is_open()) {
                    try {
                        workspaceConfig_ = nlohmann::json::parse(file2);
                        file2.close();
                    } catch (...) {
                        file2.close();
                    }
                }
            } else {
                CLogger::Warn("Restore branch {} failed: {}", target, result.stderrOutput);
            }
        }
    }

    if (!workspaceConfig_.contains("version")) {
        workspaceConfig_["version"] = ConfigVersion;
    }

    workspaceConfig_.erase("sync_target");
    workspaceConfig_.erase("directory_mapping");

    if (workspaceConfig_.contains("branches") && workspaceConfig_["branches"].is_array()) {
        for (auto& b : workspaceConfig_["branches"]) {
            b.erase("game_version");
            b.erase("modloader");
        }
    }

    ensureBranchConfigs();

    repoEditor_->loadFromJson(workspaceConfig_);
    contentIde_->setRepository(dir);

    if (workspaceConfig_.contains("branches") && workspaceConfig_["branches"].is_array()) {
        std::vector<nlohmann::json> branches;
        std::string defaultBranch;
        if (workspaceConfig_.contains("git") && workspaceConfig_["git"].contains("default_branch")) {
            defaultBranch = workspaceConfig_["git"]["default_branch"].get<std::string>();
        }
        for (auto& b : workspaceConfig_["branches"]) {
            branches.push_back(b);
        }
        branchEditor_->loadBranches(branches, defaultBranch);
    }

    updateBranchSelector();
    if (!branchCombo_->currentText().isEmpty()) {
        onBranchChanged(branchCombo_->currentText().toStdString());
    }

    modified_ = false;

    QString repoDir = QString::fromStdString(dirPath);
    int idx = recentFiles_.indexOf(repoDir);
    if (idx >= 0) recentFiles_.removeAt(idx);
    recentFiles_.prepend(repoDir);
    if (recentFiles_.size() > MaxRecentFiles) {
        recentFiles_ = recentFiles_.mid(0, MaxRecentFiles);
    }
    settings_.setValue("recentFiles2", recentFiles_);

    settings_.setValue("lastDirectory", repoDir);

    updateTitle();
    updateStatus();

    gitPanel_->setRepoPath(dirPath);
    integrityAction_->setEnabled(true);
    gitInfoAction_->setEnabled(true);
    createBranchAction_->setEnabled(true);
    switchBranchAction_->setEnabled(true);
    forkRepoAction_->setEnabled(true);
    branchMetaAction_->setEnabled(true);
    runIntegrityCheck(false);

    CLogger::Info("Open repo: {}", dirPath);
    return true;
}

bool EditorWindow::saveWorkspace()
{
    if (currentFilePath_.empty()) {
        return saveWorkspaceAs();
    }

    if (!currentFilePath_.empty()) {
        QFileInfo fi(QString::fromStdString(currentFilePath_));
        NeoWorkspace::GitOperations git;
        auto cur = git.currentBranch(fi.absolutePath().toStdString());
        QString branch = QString::fromStdString(cur.stdoutOutput).trimmed();
        if (!branch.isEmpty() && branch != "HEAD")
            workspaceConfig_["git"]["current_branch"] = branch.toStdString();
    }

    try {
        std::ofstream file(currentFilePath_);
        if (!file.is_open()) {
            QMessageBox::critical(this, "\u4fdd\u5b58\u5931\u8d25", "\u65e0\u6cd5\u5199\u5165\u6587\u4ef6\uff0c\u68c0\u67e5\u6743\u9650\u3002");
            return false;
        }
        file << workspaceConfig_.dump(2) << std::endl;
        file.close();
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "保存失败",
            QString("\u5199\u5165\u6587\u4ef6\u65f6\u53d1\u751f\u9519\u8bef:\n%1").arg(e.what()));
        return false;
    }

    modified_ = false;
    updateTitle();
    updateStatus();

    CLogger::Info("Save workspace: {}", currentFilePath_);
    return true;
}

bool EditorWindow::saveWorkspaceAs()
{
    QString lastDir = settings_.value("lastDirectory", "").toString();
    QString filepath = QFileDialog::getSaveFileName(
        this, "\u4fdd\u5b58\u5de5\u4f5c\u533a\u4e3a",
        lastDir + "/workspace.json",
        "JSON \u6587\u4ef6 (*.json);;\u6240\u6709\u6587\u4ef6 (*)");

    if (filepath.isEmpty()) return false;

    currentFilePath_ = filepath.toStdString();
    return saveWorkspace();
}

bool EditorWindow::isModified() const { return modified_; }

void EditorWindow::closeEvent(QCloseEvent* event)
{
    CLogger::Info("Close flow: closeEvent entered, modified={}", modified_ ? "yes" : "no");
    CLogger::Info("Close flow: closeEvent entry heap={}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    if (gitPanel_->hasChanges()) {
        CLogger::Info("Close flow: unsaved changes detected, showing confirm");
        auto reply = QMessageBox::warning(this, "未提交的更改",
            "Git \u5386\u53f2\u4e2d\u5b58\u5728\u672a\u63d0\u4ea4\u7684\u6539\u52a8\u3002\n\n\u786e\u8ba4\u8981\u9000\u51fa\u5417?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            CLogger::Info("Close flow: user cancelled exit");
            event->ignore();
            return;
        }
    }

    CLogger::Info("Close flow: saving window settings");
    settings_.setValue("windowGeometry", saveGeometry());
    settings_.setValue("splitterSizes", mainSplitter_->saveState());
    settings_.sync();
    if (contentIde_) {
        contentIde_->cleanupOnExit();
    }
    CLogger::Info("Close flow: heap after save={}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    CLogger::Info("Close flow: closeEvent accepted");
    event->accept();
}

void EditorWindow::onOpen()
{
    auto [dirPath, source] = selectWorkspaceFile();
    if (!dirPath.empty()) {
        loadWorkspace(dirPath, source);
    }
}

void EditorWindow::onSave()
{
    if (currentFilePath_.empty()) {
        saveWorkspaceAs();
    } else {
        saveWorkspace();
    }
}

void EditorWindow::onSaveAs()
{
    saveWorkspaceAs();
}

void EditorWindow::onVerify()
{
    if (currentFilePath_.empty()) {
        QMessageBox::information(this, "\u9a8c\u8bc1", "\u8bf7\u5148\u6253\u5f00\u6216\u4fdd\u5b58\u5de5\u4f5c\u533a\u914d\u7f6e\u6587\u4ef6\u3002");
        return;
    }

    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    if (!workspaceConfig_.contains("workspace") || !workspaceConfig_["workspace"].contains("name")) {
        errors.push_back("缺少 workspace.name 字段");
    }
    if (!workspaceConfig_.contains("git") || !workspaceConfig_["git"].contains("remote")) {
        errors.push_back("缺少 git.remote 字段");
    }
    if (!workspaceConfig_.contains("branches") || workspaceConfig_["branches"].empty()) {
        errors.push_back("branches 数组为空");
    } else {
        std::set<std::string> names;
        for (auto& b : workspaceConfig_["branches"]) {
            std::string name = b.value("name", "");
            if (name.empty()) {
                errors.push_back("\u5206\u652f\u540d\u4e0d\u80fd\u4e3a\u7a7a");
            } else if (names.count(name)) {
                errors.push_back("重复的分支名: " + name);
            }
            names.insert(name);

            std::string parent = b.value("parent", "");
            if (!parent.empty() && !names.count(parent)) {
                warnings.push_back("\u7236\u5206\u652f \u0027" + parent + "\u0027 \u672a\u5728\u5206\u652f\u5217\u8868\u4e2d\u627e\u5230\uff0c\u5c06\u5728\u7ee7\u627f\u524d\u63d0\u524d\u505c\u6b62");
            }
        }
    }

    if (errors.empty() && warnings.empty()) {
        QMessageBox::information(this, "验证通过",
            "工作区配置文件结构有效，未发现问题。\n\n"
            "  \u81ea\u5b9a\u4e49\u5b57\u6bb5\u76f8\u4e92\u77db\u76fe\n"
            "  \u53c2\u6570\u4e0d\u6ee1\u8db3\u89c4\u5219");
    } else {
        QString msg;
        if (!errors.empty()) {
            msg += "错误:\n";
            for (auto& e : errors) msg += "  \u2022 " + QString::fromStdString(e) + "\n";
        }
        if (!warnings.empty()) {
            msg += "\n警告:\n";
            for (auto& w : warnings) msg += "  \u2022 " + QString::fromStdString(w) + "\n";
        }
QMessageBox::warning(this, "验证结果", msg);
    }
}

void EditorWindow::onCheckIntegrity()
{
    runIntegrityCheck(true);
}

void EditorWindow::runIntegrityCheck(bool manual)
{
    if (currentFilePath_.empty()) {
        if (manual)
            QMessageBox::information(this, "\u5b8c\u6574\u6027\u68c0\u67e5", "\u8bf7\u5148\u6253\u5f00\u4e00\u4e2a\u4ed3\u5e93\u3002");
        return;
    }

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString dir = fi.absolutePath();
    if (!QDir(dir).exists()) return;

    auto runGit = [&](const QStringList& args) {
        QProcess proc;
        proc.setWorkingDirectory(dir);
        proc.start("git", args);
        proc.waitForFinished(15000);
        return QString::fromUtf8(proc.readAllStandardOutput());
    };

    QString statusOut = runGit({"status", "--porcelain", "-b"});

    struct Item {
        enum class Kind { Untracked, Modified, Deleted };
        Kind kind;
        QString path;
    };
    std::vector<Item> items;
    QString syncNote;

    for (auto& line : statusOut.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("## ")) {
            QRegularExpression re(R"(\[ahead (\d+)(?:, behind (\d+))?\])");
            QRegularExpression re2(R"(\[behind (\d+)\])");
            auto m = re.match(line);
            int ahead = 0, behind = 0;
            if (m.hasMatch()) {
                ahead = m.captured(1).toInt();
                auto m2 = re2.match(line);
                behind = m2.hasMatch() ? m2.captured(1).toInt() : 0;
            } else {
                auto m2 = re2.match(line);
                if (m2.hasMatch()) behind = m2.captured(1).toInt();
            }
            if (ahead > 0 && behind > 0)
                syncNote = QString("\u672c\u5730\u5df2\u8d85\u8fc7 origin %1 \u4e2a\u63d0\u4ea4\u5e76\u843d\u540e %2 \u4e2a\u63d0\u4ea4").arg(ahead).arg(behind);
            else if (ahead > 0)
                syncNote = QString("\u672c\u5730\u5df2\u8d85\u8fc7 origin %1 \u4e2a\u63d0\u4ea4\u5c1a\u672a\u63a8\u9001").arg(ahead);
            else if (behind > 0)
                syncNote = QString("\u672c\u5730\u5df2\u843d\u540e origin %1 \u4e2a\u63d0\u4ea4\u5c1a\u672a\u62c9\u53d6").arg(behind);
            else
                syncNote = "\u5df2\u4e0e origin \u540c\u6b65";
            continue;
        }
        if (line.startsWith("?? ")) {
            items.push_back({Item::Kind::Untracked, line.mid(3)});
            continue;
        }
        if (line.length() < 4) continue;
        char x = line.at(0).toLatin1();
        char y = line.at(1).toLatin1();
        QString path = line.mid(3);
        if (x == 'D' || y == 'D')
            items.push_back({Item::Kind::Deleted, path});
        else
            items.push_back({Item::Kind::Modified, path});
    }

    std::set<std::string> branchNames;
    if (workspaceConfig_.contains("branches") && workspaceConfig_["branches"].is_array()) {
        for (auto& b : workspaceConfig_["branches"])
            branchNames.insert(b.value("name", ""));
    }

    QString missingBc;
    for (auto& n : branchNames) {
        if (!QFile::exists(QString::fromStdString(branchConfigDir() + "/" + n + ".json"))) {
            missingBc = QString::fromStdString(n);
            break;
        }
    }

    QString orphanBc;
    QDir bcDir(QString::fromStdString(branchConfigDir()));
    if (bcDir.exists()) {
        for (auto& f : bcDir.entryList({"*.json"}, QDir::Files)) {
            QString bn = QFileInfo(f).baseName();
            if (!branchNames.count(bn.toStdString())) {
                orphanBc = bn;
                break;
            }
        }
    }

    QString summary;
    if (!syncNote.isEmpty()) summary = syncNote;
    if (!missingBc.isEmpty())
        summary += (summary.isEmpty() ? QString("") : QString("\n")) + QString("缺少分支配置文件: ") + missingBc;
    if (!orphanBc.isEmpty())
        summary += (summary.isEmpty() ? QString("") : QString("\n")) + QString("孤儿 branch_config 文件: ") + orphanBc;

    if (items.empty()) {
        QString msg = summary.isEmpty() ? "\u4ed3\u5e93\u5b8c\u6574\u6027\u68c0\u67e5\u65e0\u5f02\u5e38" : summary;
        if (manual)
            QMessageBox::information(this, "\u5b8c\u6574\u6027\u68c0\u67e5", msg);
        else {
            statusBar()->showMessage(msg, 6000);
            const int nMissing = missingBc.isEmpty() ? 0 : missingBc.split('\n').size();
            const int nOrphan = orphanBc.isEmpty() ? 0 : orphanBc.split('\n').size();
            CLogger::Info("Integrity check passed: {} orphan branch_config file(s), {} missing",
                nOrphan, nMissing);
        }
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("\u4ed3\u5e93\u5b8c\u6574\u6027\u68c0\u67e5");
    dlg.setMinimumSize(820, 480);

    auto* layout = new QVBoxLayout(&dlg);

    auto* summaryLabel = new QLabel(summary.isEmpty()
        ? QString("发现 %1 个待处理文件，请选择每项的操作：").arg(items.size())
        : QString("%1\n\n发现 %2 个待处理文件，请选择每项的操作：")
            .arg(summary).arg(items.size()), &dlg);
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    auto* table = new QTableWidget(&dlg);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels({"\u72b6\u6001", "\u6587\u4ef6", "\u5904\u7406"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    std::vector<std::pair<Item, QComboBox*>> rows;
    for (auto& it : items) {
        int row = table->rowCount();
        table->insertRow(row);
        QString kindStr;
        switch (it.kind) {
            case Item::Kind::Untracked: kindStr = "\u672a\u8ddf\u8e2a"; break;
            case Item::Kind::Modified:  kindStr = "\u5df2\u4fee\u6539"; break;
            case Item::Kind::Deleted:   kindStr = "\u5df2\u5220\u9664"; break;
        }
        table->setItem(row, 0, new QTableWidgetItem(kindStr));
        table->setItem(row, 1, new QTableWidgetItem(it.path));
        auto* combo = new QComboBox(&dlg);
        if (it.kind == Item::Kind::Untracked) {
            combo->addItem("\u4e0d\u5904\u7406\u8be5\u9879", "track");
            combo->addItem("忽略 (.gitignore)", "ignore");
            combo->addItem("\u4ece\u6e05\u5355\u5220\u9664", "delete");
            combo->addItem("跳过", "skip");
        } else if (it.kind == Item::Kind::Modified) {
            combo->addItem("\u4fdd\u7559 (\u4ec5\u65b0\u589e\u9879\u76ee)", "track");
            combo->addItem("\u8fd8\u539f Git \u7248\u672c", "restore");
            combo->addItem("忽略 (.gitignore)", "ignore");
            combo->addItem("跳过", "skip");
        } else {
            combo->addItem("保留 (保留删除)", "track");
            combo->addItem("还原文件", "restore");
            combo->addItem("跳过", "skip");
        }
        table->setCellWidget(row, 2, combo);
        rows.push_back({it, combo});
    }

    layout->addWidget(table);

    QDialogButtonBox* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    box->button(QDialogButtonBox::Ok)->setText("确定");
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(box);

    if (dlg.exec() != QDialog::Accepted) return;

    auto runGitFor = [&](const QStringList& args) {
        QProcess proc;
        proc.setWorkingDirectory(dir);
        proc.start("git", args);
        proc.waitForFinished(15000);
    };

    QStringList trackPaths, restorePaths, deletePaths;
    QStringList ignorePaths;
    for (auto& [it, combo] : rows) {
        QString action = combo->currentData().toString();
        if (action == "track") trackPaths << it.path;
        else if (action == "restore") restorePaths << it.path;
        else if (action == "delete") deletePaths << it.path;
        else if (action == "ignore") ignorePaths << it.path;
    }

    if (!deletePaths.isEmpty()) {
        auto reply = QMessageBox::question(this, "确认删除",
            "以下文件将从磁盘永久删除，且无法恢复：\n\n" + deletePaths.join('\n') +
            "\n\n\u786e\u8ba4\u8981\u5220\u9664\u5417?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) deletePaths.clear();
    }

    if (!trackPaths.isEmpty()) runGitFor(QStringList{"add", "--"} + trackPaths);
    if (!restorePaths.isEmpty()) runGitFor(QStringList{"checkout", "--"} + restorePaths);
    if (!ignorePaths.isEmpty() || !deletePaths.isEmpty()) {
        QFile gi(dir + "/.gitignore");
        QString appendText;
        if (gi.open(QIODevice::Append | QIODevice::Text)) {
            for (auto& p : ignorePaths) appendText += p + "\n";
            gi.write(appendText.toUtf8());
            gi.close();
        }
        if (!deletePaths.isEmpty()) {
            for (auto& p : deletePaths) {
                bool ok = QFile::remove(dir + "/" + p);
                if (!ok) ignorePaths << p;
            }
        }
    }
    if (!ignorePaths.isEmpty()) runGitFor(QStringList{"add", "--"} + ignorePaths);

    QString done;
    if (!trackPaths.isEmpty()) done += QString("%1 项已暂存\n").arg(trackPaths.size());
    if (!restorePaths.isEmpty()) done += QString("%1 项已还原\n").arg(restorePaths.size());
    if (!deletePaths.isEmpty()) done += QString("%1 项已从磁盘删除\n").arg(deletePaths.size());
    if (!ignorePaths.isEmpty()) done += QString("%1 项已加入 .gitignore\n").arg(ignorePaths.size());
    if (done.isEmpty()) done = "\u672a\u505a\u4efb\u4f55\u5904\u7406";

    QMessageBox::information(this, "\u5b8c\u6574\u6027\u68c0\u67e5", done.trimmed());
    gitPanel_->refresh();
}

void EditorWindow::onExportBackup()
{
    if (currentFilePath_.empty()) {
        QMessageBox::information(this, "\u5bfc\u51fa\u5907\u4efd", "\u8bf7\u5148\u4fdd\u5b58\u5de5\u4f5c\u533a\u914d\u7f6e\u6587\u4ef6\u3002");
        return;
    }

    QString lastDir = settings_.value("lastDirectory", "").toString();
    auto now = QDateTime::currentDateTime();
    QString defaultName = QString("workspace_backup_%1.json")
        .arg(now.toString("yyyyMMdd_HHmmss"));

    QString filepath = QFileDialog::getSaveFileName(
        this, "导出备份", lastDir + "/" + defaultName,
        "JSON \u6587\u4ef6 (*.json);;\u6240\u6709\u6587\u4ef6 (*)");

    if (filepath.isEmpty()) return;

    try {
        std::ofstream file(filepath.toStdString());
        file << workspaceConfig_.dump(2) << std::endl;
        file.close();
        QMessageBox::information(this, "导出成功",
            QString("备份已导出到:\n%1").arg(filepath));
        CLogger::Info("Export backup: {}", filepath.toStdString());
    } catch (const std::exception& e) {
        QMessageBox::critical(this, "导出失败",
            QString("无法写入备份文件:\n%1").arg(e.what()));
    }
}

void EditorWindow::onBranchChanged(const std::string& branchName)
{
    contentIde_->setBranch(QString::fromStdString(branchName),
        QString::fromStdString(branchConfigDir()));
    CLogger::Debug("Switch to branch: {}", branchName);
}

void EditorWindow::onAnyModified()
{
    if (!modified_) {
        modified_ = true;
        updateTitle();
        updateStatus();
    }
}

void EditorWindow::updateTitle()
{
    QString title = "NSUM \u4ed3\u5e93\u7ba1\u7406\u5668";
    if (!currentFilePath_.empty()) {
        QFileInfo fi(QString::fromStdString(currentFilePath_));
        title += " - " + fi.fileName();
    }
    if (modified_) {
        title += " *";
    }
    setWindowTitle(title);
}

void EditorWindow::updateStatus()
{
    if (currentFilePath_.empty()) {
        filePathLabel_->setText("未打开文件");
    } else {
        filePathLabel_->setText(QString::fromStdString(currentFilePath_));
    }

    if (modified_) {
        modifiedLabel_->setText("\u5df2\u4fee\u6539");
        modifiedLabel_->setStyleSheet("color: red; font-weight: bold;");
    } else {
        modifiedLabel_->setText("\u5df2\u4fdd\u5b58");
        modifiedLabel_->setStyleSheet("color: green;");
    }
}

std::string EditorWindow::branchConfigDir() const
{
    QFileInfo fi(QString::fromStdString(currentFilePath_));
    return (fi.absolutePath() + "/branch_config").toStdString();
}

void EditorWindow::gitAddPaths(const QStringList& paths)
{
    if (currentFilePath_.empty() || paths.isEmpty()) return;
    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QProcess proc;
    proc.setWorkingDirectory(fi.absolutePath());
    proc.start("git", QStringList{"add", "--"} + paths);
    proc.waitForFinished(15000);
}

void EditorWindow::ensureGitIgnore(const QString& dir)
{
    QString path = dir + "/.gitignore";
    if (QFile::exists(path)) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;

    f.write("# NeoServerUpdateModpack - 默认忽略规则\n");
    f.write("build/\n");
    f.write("*.log\n");
    f.write("*.cache\n");
    f.write(".cache/\n");
    f.write(".idea/\n");
    f.write(".vscode/\n");
    f.write(".DS_Store\n");
    f.write("Thumbs.db\n");
    f.close();
}

void EditorWindow::ensureBranchConfigs()
{
    std::string dir = branchConfigDir();
    if (!currentFilePath_.empty()) {
        QDir().mkpath(QString::fromStdString(dir));
    }

    if (!workspaceConfig_.contains("branches") || !workspaceConfig_["branches"].is_array())
        return;

    bool hasLegacy = workspaceConfig_.contains("file_manifest");
    nlohmann::json legacy;
    if (hasLegacy) {
        legacy["file_manifest"] = workspaceConfig_.value("file_manifest", nlohmann::json::object());
        legacy["pointer_files"] = workspaceConfig_.value("pointer_files", nlohmann::json::object());
        legacy["serverconfig_sync"] = workspaceConfig_.value("serverconfig_sync",
            nlohmann::json::object());
        legacy["custom_mods"] = workspaceConfig_.value("custom_mods",
            nlohmann::json::object());
    }

    auto defaultConfig = []() {
        nlohmann::json j;
        j["file_manifest"] = nlohmann::json::object();
        j["pointer_files"] = nlohmann::json::object();
        j["serverconfig_sync"] = {
            {"enabled", false},
            {"scan_paths", nlohmann::json::array()},
            {"rules", nlohmann::json::array()}
        };
        j["custom_mods"] = {
            {"enabled", false},
            {"path", ""},
            {"merge_order", "post"}
        };
        return j;
    };

    for (auto& b : workspaceConfig_["branches"]) {
        std::string name = b.value("name", "");
        if (name.empty()) continue;

        std::string filePath = dir + "/" + name + ".json";
        if (QFile::exists(QString::fromStdString(filePath)))
            continue;

        nlohmann::json entry = hasLegacy ? legacy : defaultConfig();
        if (!entry.contains("serverconfig_sync") || entry["serverconfig_sync"].is_null()) {
            entry["serverconfig_sync"] = defaultConfig()["serverconfig_sync"];
        }
        if (!entry.contains("custom_mods") || entry["custom_mods"].is_null()) {
            entry["custom_mods"] = defaultConfig()["custom_mods"];
        }

        std::ofstream f(filePath);
        f << entry.dump(2) << std::endl;
        f.close();
        branchConfigs_[name] = std::move(entry);
    }

    gitAddPaths({QString::fromStdString(dir)});

    if (hasLegacy) {
        workspaceConfig_.erase("file_manifest");
        workspaceConfig_.erase("pointer_files");
        workspaceConfig_.erase("serverconfig_sync");
        workspaceConfig_.erase("custom_mods");
        modified_ = true;
        saveWorkspace();
    }
}

nlohmann::json EditorWindow::branchConfig(const std::string& name)
{
    auto it = branchConfigs_.find(name);
    if (it != branchConfigs_.end()) return it->second;

    std::string filePath = branchConfigDir() + "/" + name + ".json";
    QFileInfo fi(QString::fromStdString(filePath));
    if (fi.exists()) {
        std::ifstream f(filePath);
        if (f.is_open()) {
            try {
                auto j = nlohmann::json::parse(f);
                branchConfigs_[name] = j;
                return j;
            } catch (...) {}
        }
    }

    nlohmann::json j;
    j["file_manifest"] = nlohmann::json::object();
    j["pointer_files"] = nlohmann::json::object();
    j["serverconfig_sync"] = {{"enabled", false},
        {"scan_paths", nlohmann::json::array()}, {"rules", nlohmann::json::array()}};
    j["custom_mods"] = {{"enabled", false}, {"path", ""}, {"merge_order", "post"}};
    branchConfigs_[name] = j;
    return j;
}

void EditorWindow::saveBranchConfig(const std::string& name)
{
    auto it = branchConfigs_.find(name);
    if (it == branchConfigs_.end()) return;
    if (currentFilePath_.empty()) return;

    std::string dir = branchConfigDir();
    QDir().mkpath(QString::fromStdString(dir));
    std::string filePath = dir + "/" + name + ".json";
    std::ofstream f(filePath);
    f << it->second.dump(2) << std::endl;
    f.close();
    gitAddPaths({QString::fromStdString(filePath)});
}

void EditorWindow::updateBranchSelector()
{
    branchCombo_->blockSignals(true);
    branchCombo_->clear();

    if (workspaceConfig_.contains("branches") && workspaceConfig_["branches"].is_array()) {
        branchCombo_->setEnabled(true);
        for (auto& b : workspaceConfig_["branches"]) {
            branchCombo_->addItem(QString::fromStdString(b.value("name", "")));
        }
    } else {
        branchCombo_->setEnabled(false);
    }

    branchCombo_->blockSignals(false);
}

bool EditorWindow::maybeSave()
{
    if (!modified_) return true;

    auto result = QMessageBox::warning(this, "未保存的更改",
        "\u5f53\u524d\u914d\u7f6e\u5df2\u88ab\u4fee\u6539\u3002\n\n\u662f\u5426\u4fdd\u5b58\u4fee\u6539?",
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

    switch (result) {
        case QMessageBox::Save:
            return saveWorkspace();
        case QMessageBox::Discard:
            return true;
        case QMessageBox::Cancel:
        default:
            return false;
    }
}

std::pair<std::string, RepoSource> EditorWindow::selectWorkspaceFile()
{
    QString lastDir = settings_.value("lastDirectory", "").toString();

    QDialog dlg(this);
    dlg.setWindowTitle("打开仓库");
    dlg.setMinimumWidth(520);

    auto* layout = new QVBoxLayout(&dlg);

    auto* modeGroup = new QGroupBox("来源", &dlg);
    auto* modeLayout = new QVBoxLayout(modeGroup);
    auto* localRadio = new QRadioButton("打开本地仓库目录", &dlg);
    auto* cloneRadio = new QRadioButton("克隆远程仓库", &dlg);
    auto* cacheRadio = new QRadioButton("\u8fdc\u7a0b\u4ed3\u5e93\u7684\u672c\u5730\u7f13\u5b58", &dlg);
    localRadio->setChecked(true);
    modeLayout->addWidget(localRadio);
    modeLayout->addWidget(cloneRadio);
    modeLayout->addWidget(cacheRadio);

    auto* dirEdit = new QLineEdit(&dlg);
    dirEdit->setPlaceholderText("仓库目录路径");
    dirEdit->setReadOnly(true);

    auto* dirBtn = new QPushButton("选择目录...", &dlg);
    auto* dirRow = new QHBoxLayout();
    dirRow->addWidget(dirEdit);
    dirRow->addWidget(dirBtn);

    auto* urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText("git@github.com:user/repo.git \u6216 https://...");
    urlEdit->setEnabled(false);

    auto* protoLabel = new QLabel(&dlg);
    protoLabel->setEnabled(false);
    protoLabel->setStyleSheet("font-size: 10px;");

    auto* remoteCombo = new QComboBox(&dlg);
    remoteCombo->setEnabled(false);
    for (const auto& f : recentFiles_) {
        remoteCombo->addItem(f);
    }
    remoteCombo->setCurrentIndex(-1);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Open | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Open)->setEnabled(false);

    layout->addWidget(modeGroup);
    layout->addSpacing(8);
    layout->addWidget(new QLabel("选择目录:", &dlg));
    layout->addLayout(dirRow);
    layout->addWidget(new QLabel("远程 URL:", &dlg));
    layout->addWidget(urlEdit);
    layout->addWidget(protoLabel);
    layout->addWidget(new QLabel("\u5df2\u7ecf\u62e5\u6709\u7684\u4ed3\u5e93", &dlg));
    layout->addWidget(remoteCombo);
    layout->addWidget(btnBox);

    auto updateOpenBtn = [&]() {
        bool ok = false;
        if (localRadio->isChecked())
            ok = !dirEdit->text().isEmpty();
        else if (cloneRadio->isChecked())
            ok = !urlEdit->text().trimmed().isEmpty()
                && detectProtocol(urlEdit->text().trimmed()) != GitProtocol::Unknown;
        else
            ok = remoteCombo->currentIndex() >= 0;
        btnBox->button(QDialogButtonBox::Open)->setEnabled(ok);
    };

    QObject::connect(localRadio, &QRadioButton::toggled, [&](bool checked) {
        dirEdit->setEnabled(checked);
        dirBtn->setEnabled(checked);
        urlEdit->setEnabled(false);
        protoLabel->setEnabled(false);
        remoteCombo->setEnabled(false);
        updateOpenBtn();
    });
    QObject::connect(cloneRadio, &QRadioButton::toggled, [&](bool checked) {
        dirEdit->setEnabled(false);
        dirBtn->setEnabled(false);
        urlEdit->setEnabled(checked);
        protoLabel->setEnabled(checked);
        remoteCombo->setEnabled(false);
        updateOpenBtn();
    });
    QObject::connect(cacheRadio, &QRadioButton::toggled, [&](bool checked) {
        dirEdit->setEnabled(false);
        dirBtn->setEnabled(false);
        urlEdit->setEnabled(false);
        protoLabel->setEnabled(false);
        remoteCombo->setEnabled(checked);
        updateOpenBtn();
    });

    QObject::connect(dirBtn, &QPushButton::clicked, [&]() {
        QString d = QFileDialog::getExistingDirectory(&dlg, "选择仓库目录", lastDir);
        if (!d.isEmpty()) {
            dirEdit->setText(d);
            updateOpenBtn();
        }
    });

    QObject::connect(urlEdit, &QLineEdit::textChanged, [&](const QString& t) {
        auto p = detectProtocol(t.trimmed());
        protoLabel->setText("协议: " + protocolLabel(p));
        protoLabel->setStyleSheet(p == GitProtocol::Unknown && !t.isEmpty()
            ? "color: red; font-size: 10px;" : "color: green; font-size: 10px;");
        updateOpenBtn();
    });

    QObject::connect(remoteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [&](int) { updateOpenBtn(); });

    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return {};

    if (localRadio->isChecked())
        return {dirEdit->text().toStdString(), RepoSource::Local};

    if (cloneRadio->isChecked()) {
        QString url = urlEdit->text().trimmed();
        if (url.isEmpty()) return {};

        QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(cacheDir);
        QString repoName = repoNameFromUrl(url);
        QString targetDir = cacheDir + "/NSUM/" + repoName;

        NeoWorkspace::GitOperations git;
        auto result = git.clone(url.toStdString(), targetDir.toStdString());
        if (result.exitCode == 0)
            return {targetDir.toStdString(), RepoSource::Clone};

        QMessageBox::critical(this, "克隆失败",
            QString("无法克隆:\n%1\n\n%2").arg(url, QString::fromStdString(result.stderrOutput)));
        return {};
    }

    if (remoteCombo->currentIndex() >= 0)
        return {remoteCombo->currentText().toStdString(), RepoSource::RemoteCache};

    return {};
}

static void autoCommitAndPush(const QString& repoDir, const QString& remoteUrl,
    const QString& branch = "main")
{
    auto runGit = [&](const QStringList& args) {
        QProcess proc;
        proc.setWorkingDirectory(repoDir);
        proc.start("git", args);
        proc.waitForFinished(30000);
        return QString::fromUtf8(proc.readAllStandardError());
    };

    runGit({"add", "-A"});
    QString err = runGit({"commit", "-m",
        "initial neoserverupdatemodpack repository, By Editor"});
    if (remoteUrl.isEmpty()) return;

    err = runGit({"push", "-u", "origin", branch});
    CLogger::Info("Git: initial push {} -> {}", branch.toStdString(), remoteUrl.toStdString());
}

void EditorWindow::onNewRepo()
{
    QDialog dlg(this);
    dlg.setWindowTitle("新建仓库");
    dlg.setMinimumWidth(500);

    auto* layout = new QVBoxLayout(&dlg);

    auto* nameEdit = new QLineEdit(&dlg);
    nameEdit->setPlaceholderText("\u4ed3\u5e93\u540d\u79f0 (\u9ed8\u8ba4\u4f7f\u7528\u76ee\u5f55\u540d)");

    auto* pathEdit = new QLineEdit(&dlg);
    pathEdit->setPlaceholderText("仓库本地路径...");
    pathEdit->setReadOnly(true);

    auto* pathBtn = new QPushButton("选择目录...", &dlg);
    auto* pathRow = new QHBoxLayout();
    pathRow->addWidget(pathEdit);
    pathRow->addWidget(pathBtn);

    auto* remoteCheck = new QCheckBox("配置远程仓库地址", &dlg);

    auto* urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText("git@github.com:user/repo.git \u6216 https://...");
    urlEdit->setEnabled(false);

    auto* protoLabel = new QLabel(&dlg);
    protoLabel->setEnabled(false);

    auto* branchEdit = new QLineEdit("main", &dlg);
    branchEdit->setPlaceholderText("\u9ed8\u8ba4\u5206\u652f");

    auto* formLayout = new QFormLayout();
    formLayout->addRow("仓库名称:", nameEdit);
    formLayout->addRow("本地路径:", pathRow);
    formLayout->addRow(remoteCheck);
    formLayout->addRow("远程 URL:", urlEdit);
    formLayout->addRow(protoLabel);
    formLayout->addRow("默认分支:", branchEdit);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    layout->addLayout(formLayout);
    layout->addWidget(btnBox);

    QObject::connect(pathBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&dlg, "选择仓库目录",
            settings_.value("lastDirectory", "").toString());
        if (!dir.isEmpty()) {
            pathEdit->setText(dir);
            if (nameEdit->text().isEmpty())
                nameEdit->setText(QFileInfo(dir).fileName());
            btnBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        }
    });

    QObject::connect(remoteCheck, &QCheckBox::toggled, [&](bool checked) {
        urlEdit->setEnabled(checked);
        protoLabel->setEnabled(checked);
    });

    QObject::connect(urlEdit, &QLineEdit::textChanged, [&](const QString& text) {
        auto proto = detectProtocol(text.trimmed());
        protoLabel->setText("\u534f\u8bae\u68c0\u6d4b " + protocolLabel(proto));
        protoLabel->setStyleSheet(proto == GitProtocol::Unknown && !text.isEmpty()
            ? "color: red;" : "color: green;");
    });

    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QString dir = pathEdit->text();
    QString repoName = nameEdit->text().isEmpty()
        ? QFileInfo(dir).fileName() : nameEdit->text();
    QString remoteUrl = urlEdit->text().trimmed();
    QString defaultBranch = branchEdit->text().trimmed();
    if (defaultBranch.isEmpty()) defaultBranch = "main";

    NeoWorkspace::GitOperations git;
    auto result = git.init(dir.toStdString());

    if (result.exitCode != 0) {
        QMessageBox::critical(this, "\u521d\u59cb\u5316\u5931\u8d25",
            QString("git init 失败:\n%1").arg(QString::fromStdString(result.stderrOutput)));
        return;
    }
    ensureGitIgnore(dir);

    if (!remoteUrl.isEmpty()) {
        auto remoteResult = git.addRemote(dir.toStdString(), "origin", remoteUrl.toStdString());
        if (remoteResult.exitCode != 0) {
            QMessageBox::warning(this, "远程配置警告",
                QString("仓库已初始化，但远程配置失败:\n%1\n\n"
                "\u53ef\u4ee5\u5728\u4ed3\u5e93\u4e2d\u624b\u52a8\u521b\u5efa\u540e\u518d\u8bd5\u3002")
                    .arg(QString::fromStdString(remoteResult.stderrOutput)));
        }
    }

    nlohmann::json ws;
    ws["workspace"]["name"] = repoName.toStdString();
    ws["workspace"]["minecraft"] = "1.21.4";
    ws["workspace"]["modloader"] = "fabric";
    ws["git"]["remote"] = remoteUrl.toStdString();
    ws["git"]["default_branch"] = defaultBranch.toStdString();
    ws["git"]["current_branch"] = defaultBranch.toStdString();
    ws["branches"] = nlohmann::json::array();
    ws["file_manifest"] = nlohmann::json::object();

    QString wsPath = dir + "/workspace.json";
    std::ofstream f(wsPath.toStdString());
    f << ws.dump(2) << std::endl;
    f.close();

    CLogger::Info("New repo: {} (remote={})", dir.toStdString(), remoteUrl.toStdString());

    QMessageBox::information(this, "\u4ed3\u5e93\u5df2\u521b\u5efa",
        QString("\u4ed3\u5e93\u521d\u59cb\u5316\u5b8c\u6210:\n%1\n\n"
                "远程: %2\n"
                "分支: %3")
            .arg(dir,
                 remoteUrl.isEmpty() ? "\u65e0" : remoteUrl,
                 defaultBranch));
    autoCommitAndPush(dir, remoteUrl, defaultBranch);
    loadWorkspace(dir.toStdString(), RepoSource::New);
}

void EditorWindow::onCloneRepo()
{
    QDialog dlg(this);
    dlg.setWindowTitle("克隆远程仓库");
    dlg.setMinimumWidth(520);

    auto* layout = new QVBoxLayout(&dlg);

    auto* urlLabel = new QLabel("仓库 URL (支持 SSH / HTTPS / HTTP):", &dlg);
    auto* urlEdit = new QLineEdit(&dlg);
    urlEdit->setPlaceholderText("git@github.com:user/repo.git  \u6216  https://github.com/user/repo.git");

    auto* protoHintLabel = new QLabel(&dlg);
    protoHintLabel->setStyleSheet("color: gray; font-size: 11px;");

    auto* examplesLabel = new QLabel(
        "SSH:    git@github.com:owner/repo.git\n"
        "HTTPS:  https://github.com/owner/repo.git\n"
        "HTTP:   http://git.example.com/owner/repo.git", &dlg);
    examplesLabel->setStyleSheet("color: #888; font-size: 10px;");

    auto* pathEdit = new QLineEdit(&dlg);
    pathEdit->setPlaceholderText("\u76ee\u6807\u7236\u76ee\u5f55..");
    pathEdit->setReadOnly(true);

    auto* pathBtn = new QPushButton("选择...", &dlg);
    auto* pathRow = new QHBoxLayout();
    pathRow->addWidget(pathEdit);
    pathRow->addWidget(pathBtn);

    auto* targetLabel = new QLabel(&dlg);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    btnBox->button(QDialogButtonBox::Ok)->setText("克隆");
    btnBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    layout->addWidget(urlLabel);
    layout->addWidget(urlEdit);
    layout->addWidget(protoHintLabel);
    layout->addSpacing(4);
    layout->addWidget(examplesLabel);
    layout->addSpacing(8);
    layout->addWidget(new QLabel("\u76ee\u6807\u7236\u76ee\u5f55", &dlg));
    layout->addLayout(pathRow);
    layout->addWidget(targetLabel);
    layout->addWidget(btnBox);

    auto updateProto = [&](const QString& text) {
        auto proto = detectProtocol(text.trimmed());
        QString hint;
        switch (proto) {
            case GitProtocol::SSH:
                hint = "\u2022 SSH \u534f\u8bae \u2022 \u9700\u8981\u751f\u6210\u5e76\u4f7f\u7528 SSH \u5bc6\u94a5\uff0c\u5728\u4ed3\u5e93\u914d\u7f6e SSH \u5bc6\u94a5\u540e\u53ef\u7528"; break;
            case GitProtocol::HTTPS:
                hint = "\u2022 HTTPS \u534f\u8bae \u2022 \u767b\u5f55\u65f6\u9700\u8981\u4f7f\u7528\u8d26\u53f7\u5bc6\u7801\u6216 Token"; break;
            case GitProtocol::HTTP:
                hint = "\u2022 HTTP \u534f\u8bae \u2022 \u660e\u6587\u4f20\u8f93\uff0c\u4e0d\u5efa\u8bae\u4f7f\u7528"; break;
            case GitProtocol::Unknown:
                hint = "\u2022 \u65e0\u6cd5\u8bc6\u522b URL \u534f\u8bae\u683c\u5f0f"; break;
            default: break;
        }
        protoHintLabel->setText(hint);
        protoHintLabel->setStyleSheet(
            (proto == GitProtocol::Unknown && !text.isEmpty())
                ? "color: red; font-size: 11px;" : "color: green; font-size: 11px;");

        QString repoName = repoNameFromUrl(text.trimmed());
        if (!pathEdit->text().isEmpty() && !repoName.isEmpty()) {
            targetLabel->setText("将克隆到: " + pathEdit->text() + "/" + repoName);
        }
        btnBox->button(QDialogButtonBox::Ok)->setEnabled(
            !text.trimmed().isEmpty() && !pathEdit->text().isEmpty()
            && proto != GitProtocol::Unknown);
    };

    QObject::connect(urlEdit, &QLineEdit::textChanged, updateProto);

    QObject::connect(pathBtn, &QPushButton::clicked, [&]() {
        QString d = QFileDialog::getExistingDirectory(&dlg, "\u9009\u62e9\u65b0\u76ee\u5f55\u7236\u76ee\u5f55",
            settings_.value("lastDirectory", "").toString());
        if (!d.isEmpty()) {
            pathEdit->setText(d);
            updateProto(urlEdit->text());
        }
    });

    QObject::connect(btnBox, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btnBox, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QString url = urlEdit->text().trimmed();
    QString parentDir = pathEdit->text().trimmed();
    QString repoName = repoNameFromUrl(url);
    QString targetDir = parentDir + "/" + repoName;

    auto& settings = settings_;
    QString lastDir = QFileInfo(parentDir).absolutePath();
    if (!lastDir.isEmpty()) settings.setValue("lastDirectory", lastDir);

    NeoWorkspace::GitOperations git;
    QMessageBox::information(this, "正在克隆...",
        QString("\u6b63\u5728\u514b\u9686 %1\n\u5230 %2\n\n\u8bf7\u7a0d\u5019..").arg(url, targetDir));

    auto result = git.clone(url.toStdString(), targetDir.toStdString());

    if (result.exitCode == 0) {
        QDir wsDir(targetDir);
        QStringList wsFiles = wsDir.entryList({"workspace.json"}, QDir::Files);
        if (!wsFiles.isEmpty()) {
            QMessageBox::information(this, "克隆完成",
                QString("\u4ed3\u5e93\u5df2\u514b\u9686\u5230:\n%1\n\nworkspace.json \u672a\u627e\u5230").arg(targetDir));
            loadWorkspace(targetDir.toStdString(), RepoSource::Clone);
        } else {
            auto reply = QMessageBox::question(this, "克隆完成",
                QString("\u4ed3\u5e93\u5df2\u514b\u9686\u5230:\n%1\n\n\u672a\u627e\u5230 workspace.json\u3002\n\u662f\u5426\u521b\u5efa\u4e00\u4e2a").arg(targetDir),
                QMessageBox::Yes | QMessageBox::No);
            if (reply == QMessageBox::Yes) {
                nlohmann::json ws;
                ws["workspace"]["name"] = repoName.toStdString();
                ws["workspace"]["minecraft"] = "1.21.4";
                ws["workspace"]["modloader"] = "fabric";
                ws["git"]["remote"] = url.toStdString();
                ws["git"]["default_branch"] = "main";
                ws["git"]["current_branch"] = "main";
                ws["branches"] = nlohmann::json::array();
                ws["file_manifest"] = nlohmann::json::object();

                QString wsPath = targetDir + "/workspace.json";
                std::ofstream f(wsPath.toStdString());
                f << ws.dump(2) << std::endl;
                f.close();
                autoCommitAndPush(targetDir, url);
                loadWorkspace(targetDir.toStdString(), RepoSource::Clone);
            }
        }
    } else {
        auto proto = detectProtocol(url);
        QString extra;
        if (proto == GitProtocol::SSH) {
            extra = "\n\nSSH 克隆失败常见原因:\n"
                    "  1. \u672a\u914d\u7f6e SSH \u5bc6\u94a5 (\u4ed3\u5e93\u9700\u914d\u7f6e SSH \u5bc6\u94a5\u540e\u53ef\u7528)\n"
                    "  2. 公钥未添加到 GitHub/GitLab\n"
                    "  3. \u4ed3\u5e93\u4e0d\u5b58\u5728\u6216\u65e0\u6cd5\u8bbf\u95ee\u6743\u9650";
        } else if (proto == GitProtocol::HTTPS || proto == GitProtocol::HTTP) {
            extra = "\n\nHTTPS 克隆失败常见原因:\n"
                    "  1. 仓库地址错误\n"
                    "  2. 仓库为私有且需要认证\n"
                    "  3. 网络连接问题";
        }
        QMessageBox::critical(this, "克隆失败",
            QString("git clone 失败:\n%1%2")
                .arg(QString::fromStdString(result.stderrOutput), extra));
    }
}

void EditorWindow::onSshKeys()
{
    auto defaultPath = QString::fromStdString(NeoWorkspace::GitOperations::defaultSshKeyPath());
    QString privPath = QFileInfo::exists(defaultPath) ? defaultPath : "";
    QString pubKey;
    bool hasKey = false;

    if (!privPath.isEmpty()) {
        pubKey = QString::fromStdString(NeoWorkspace::GitOperations::readPublicKey(privPath.toStdString()));
        hasKey = !pubKey.isEmpty();
    }

    QString msg;
    if (hasKey) {
        msg = QString("SSH key found at: %1\n\nPublic key:\n%2\n\nCopy this key to GitHub/GitLab SSH settings.")
            .arg(privPath, pubKey);
    } else {
        msg = QString("No SSH key found at: %1\n\nGenerate a new key?").arg(defaultPath);
    }

    QMessageBox dlg(this);
    dlg.setWindowTitle("SSH Key Manager");
    dlg.setText(msg);

    auto* genBtn = dlg.addButton("Generate New Key", QMessageBox::ActionRole);
    auto* testBtn = dlg.addButton("Test Connection", QMessageBox::ActionRole);
    auto* copyBtn = hasKey ? dlg.addButton("Copy Public Key", QMessageBox::ActionRole) : nullptr;
    auto* closeBtn = dlg.addButton(QMessageBox::Close);

    dlg.exec();
    auto* clicked = dlg.clickedButton();

    if (clicked == genBtn) {
        bool ok;
        QString comment = QInputDialog::getText(this, "SSH Key Comment",
            "Comment (e.g. your email):", QLineEdit::Normal, "", &ok);
        if (!ok) return;

        NeoWorkspace::GitOperations git;
        auto result = git.generateSshKey(defaultPath.toStdString(),
            comment.toStdString());

        if (result.exitCode == 0) {
            QString pub = QString::fromStdString(
                NeoWorkspace::GitOperations::readPublicKey(defaultPath.toStdString()));
            QMessageBox::information(this, "SSH Key Generated",
                QString("Key saved to: %1\n\nPublic key:\n%2\n\n"
                    "Add this to GitHub: Settings \u2192 SSH and GPG keys \u2192 New SSH Key")
                .arg(defaultPath, pub));
        } else {
            QMessageBox::critical(this, "Error",
                QString("Failed to generate SSH key:\n%1")
                    .arg(QString::fromStdString(result.stderrOutput)));
        }
    } else if (clicked == testBtn) {
        NeoWorkspace::GitOperations git;
        QString path = privPath.isEmpty() ? defaultPath : privPath;
        git.generateSshKey(path.toStdString()); // idempotent if exists

        QMessageBox info(this);
        info.setWindowTitle("Testing SSH...");
        info.setText("Testing SSH connection to github.com...");
        info.show();
        QApplication::processEvents();

        auto result = git.testSshConnection();
        info.close();

        if (result.exitCode == 0 || result.stdoutOutput.find("successfully authenticated") != std::string::npos) {
            QMessageBox::information(this, "SSH Test", "SSH connection to github.com successful!");
        } else {
            QMessageBox::information(this, "SSH Test",
                QString("SSH test result:\n%1\n\n%2\n\n"
                    "Make sure your public key is added to GitHub.")
                    .arg(QString::fromStdString(result.stdoutOutput),
                         QString::fromStdString(result.stderrOutput)));
        }
    } else if (clicked == copyBtn && hasKey) {
        QApplication::clipboard()->setText(pubKey);
        QMessageBox::information(this, "Copied", "Public key copied to clipboard.");
    }
}

void EditorWindow::onGitInfo()
{
    if (currentFilePath_.empty()) return;

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString repoDir = fi.absolutePath();

    NeoWorkspace::GitOperations git;
    auto cur = git.currentBranch(repoDir.toStdString());
    QString branch = QString::fromStdString(cur.stdoutOutput).trimmed();
    if (branch.isEmpty() || branch == "HEAD") branch = "(detached)";

    QProcess proc;
    proc.setWorkingDirectory(repoDir);
    proc.start("git", {"remote", "get-url", "origin"});
    proc.waitForFinished(10000);
    QString remote = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (remote.isEmpty()) remote = "(\u65e0\u8fdc\u7a0b)";

    auto st = git.status(repoDir.toStdString());
    QString status = QString::fromStdString(st.stdoutOutput).trimmed();
    QString dirty = status.isEmpty() ? "\u5e72\u51c0" : QString("\u672a\u63d0\u4ea4\u6539\u52a8 (%1 \u9879)").arg(status.count('\n') + 1);

    QMessageBox::information(this, "Git 信息",
        QString("仓库路径: %1\n\n"
                "Git \u5df2\u6267\u884c\u5b8c\u6bd5\u3002 %2\n\n"
                "当前分支: %3\n\n"
                "远程仓库: %4\n\n"
                "\u5f53\u524d\u72b6\u6001\u3002 %5")
            .arg(repoDir,
                 QString::fromStdString(NeoWorkspace::GitOperations::GetDefaultGitPath()),
                 branch, remote, dirty));
}

void EditorWindow::onCreateBranch()
{
    if (currentFilePath_.empty()) return;

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString repoDir = fi.absolutePath();

    NeoWorkspace::GitOperations git;
    auto cur = git.currentBranch(repoDir.toStdString());
    QString base = QString::fromStdString(cur.stdoutOutput).trimmed();
    if (base.isEmpty() || base == "HEAD") base = "main";

    bool ok = false;
    QString name = QInputDialog::getText(this, "\u521b\u5efa\u65b0\u5206\u652f",
        QString("\u5f53\u524d\u5206\u652f: %1\n\n\u65b0\u5206\u652f\u540d\u79f0:").arg(base),
        QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    name = name.trimmed();
    auto result = git.createBranch(repoDir.toStdString(), name.toStdString(), base.toStdString());
    if (result.exitCode != 0) {
        QMessageBox::critical(this, "创建分支失败",
            QString("git branch %1 失败:\n%2").arg(name,
                QString::fromStdString(result.stderrOutput)));
        return;
    }

    QMessageBox::information(this, "\u5206\u652f\u5df2\u521b\u5efa",
        QString("分支 %1 已创建（基于 %2）。\n\n"
            "\u5c1a\u672a\u5207\u6362\u5230\u8be5\u5206\u652f\uff0c\u53ef\u4ee5\u5728\u5207\u6362\u540e\u4f7f\u7528\u3002\u5207\u6362\u5206\u652f\u540e\u518d\u8bd5\u3002")
            .arg(name, base));
    gitPanel_->refresh();
}

void EditorWindow::onSwitchBranch()
{
    if (currentFilePath_.empty()) return;

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString repoDir = fi.absolutePath();

    NeoWorkspace::GitOperations git;
    auto cur = git.currentBranch(repoDir.toStdString());
    QString current = QString::fromStdString(cur.stdoutOutput).trimmed();

    auto local = git.listBranches(repoDir.toStdString());
    QStringList branches;
    for (auto& line : QString::fromStdString(local.stdoutOutput).split('\n', Qt::SkipEmptyParts)) {
        QString b = line.trimmed();
        if (b.startsWith("* ")) b = b.mid(2);
        b.remove(QRegularExpression("^\\s+"));
        if (!b.isEmpty() && !b.contains("->"))
            branches.append(b);
    }
    if (branches.isEmpty()) {
        QMessageBox::information(this, "\u5207\u6362\u5206\u652f", "\u5f53\u524d\u5df2\u5728\u8be5\u5206\u652f\u4e0a\u3002");
        return;
    }
    branches.removeAll(current);

    bool ok = false;
    QString target = QInputDialog::getItem(this, "切换分支",
        QString("\u5f53\u524d\u5206\u652f: %1\n\n\u9009\u62e9\u8981\u5207\u6362\u5230\u7684\u5206\u652f:").arg(current),
        branches, 0, false, &ok);
    if (!ok || target.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto result = git.checkout(repoDir.toStdString(), target.toStdString());
    QApplication::restoreOverrideCursor();
    if (result.exitCode != 0) {
        QMessageBox::critical(this, "切换分支失败",
            QString("git checkout %1 失败:\n%2").arg(target,
                QString::fromStdString(result.stderrOutput)));
        return;
    }

    workspaceConfig_["git"]["current_branch"] = target.toStdString();
    saveWorkspace();
    gitPanel_->refresh();

    if (QMessageBox::question(this, "切换完成",
        QString("\u5df2\u5207\u6362\u5230\u5206\u652f %1\u3002\n\n\u662f\u5426\u91cd\u65b0\u52a0\u8f7d\u5de5\u4f5c\u533a\u914d\u7f6e?").arg(target),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        loadWorkspace(repoDir.toStdString(), RepoSource::Local);
    }
}

void EditorWindow::onForkRepo()
{
    if (currentFilePath_.empty()) return;

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString srcDir = fi.absolutePath();

    QString target = QFileDialog::getExistingDirectory(this, "选择分叉目标目录", srcDir);
    if (target.isEmpty()) return;

    QString name = QFileInfo(srcDir).fileName();
    if (QFile::exists(target + "/" + name)) {
        QMessageBox::warning(this, "\u76ee\u5f55\u5df2\u5b58\u5728",
            QString("\u76ee\u6807\u76ee\u5f55\u5df2\u5b58\u5728:\n%1/%2").arg(target, name));
        return;
    }

    QMessageBox info(this);
    info.setWindowTitle("\u672c\u5730\u514b\u9686..");
    info.setText(QString("\u6b63\u5728\u514b\u9686 %1 \u5230 %2/%3 ...").arg(srcDir, target, name));
    info.show();
    QApplication::processEvents();

    NeoWorkspace::GitOperations git;
    auto result = git.clone(srcDir.toStdString(),
        (target + "/" + name).toStdString());
    info.close();

    if (result.exitCode != 0) {
        QMessageBox::critical(this, "分叉失败",
            QString("克隆失败:\n%1")
                .arg(QString::fromStdString(result.stderrOutput)));
        return;
    }

    QString newWs = target + "/" + name + "/workspace.json";
    if (QMessageBox::question(this, "分叉完成",
        QString("已分叉到:\n%1/%2\n\n是否在编辑器中打开分叉副本?").arg(target, name),
        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        loadWorkspace((target + "/" + name).toStdString(), RepoSource::Local);
    }
}

void EditorWindow::onBranchMeta()
{
    if (currentFilePath_.empty()) return;

    QFileInfo fi(QString::fromStdString(currentFilePath_));
    QString repoDir = fi.absolutePath();

    BranchMetaDialog dlg(repoDir, this);
    dlg.exec();
    gitPanel_->refresh();
}

// moc handled by AUTOMOC
