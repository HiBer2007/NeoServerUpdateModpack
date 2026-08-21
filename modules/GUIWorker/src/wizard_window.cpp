#include "wizard_window.h"
#include "repo_page.h"
#include "branch_page.h"
#include "modpack_page.h"
#include "export_type_page.h"
#include "export_dir_page.h"
#include "extra_info_page.h"
#include "build_checklist_page.h"
#include "build_page.h"
#include "done_page.h"
#include "toast_notification.h"
#include <build_engine.h>
#include <git_operations.h>
#include <QDesktopServices>
#include <QUrl>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QEasingCurve>
#include <QDir>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QApplication>
#include <QUrl>
#include <QFileInfo>
#include <QDesktopServices>

#include <powerhelper_bridge.h>
#include <algorithm>
#include <iostream>
#include <functional>
#include <crtdbg.h>
#include <logger.h>
#include <nlohmann/json.hpp>

namespace {

static void triggerStackOverflow(volatile int depth)
{
    volatile char pad[1024];
    pad[0] = (char)(depth & 0xFF);
    if (depth > 0) triggerStackOverflow(depth - 1);
}

class ClickableLabel : public QLabel {
public:
    using QLabel::QLabel;

    void setClicked(const std::function<void()>& fn) {
        clicked_ = fn;
    }

protected:
    void mousePressEvent(QMouseEvent* ev) override {
        QLabel::mousePressEvent(ev);
        if (clicked_)
            clicked_();
    }

private:
    std::function<void()> clicked_;
};

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
        setText(QString("[%1] 释放取消 — %2 秒后崩溃").arg(crashType_).arg(count_));
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

namespace GUIWorker {

WizardWindow::WizardWindow(QWidget* parent)
    : QMainWindow(parent)
    , currentPage_(PAGE_REPO)
{
    CLogger::Info("Wizard construct: entry={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    setWindowTitle(QString::fromUtf8("NSUM构建工具"));
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    CLogger::Info("Wizard construct: after flags={}", _CrtCheckMemory() ? "OK" : "CORRUPT");

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    CLogger::Info("Wizard construct: after central={}", _CrtCheckMemory() ? "OK" : "CORRUPT");

    const int defaultSp = mainLayout->spacing();
    mainLayout->setSpacing((defaultSp * 3) / 10);

    stack_ = new QStackedWidget(this);
    CLogger::Info("Wizard construct: after stack={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    repoPage_ = new RepoPage(this);
    CLogger::Info("Construct layout: after repoPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    branchPage_ = new BranchPage(this);
    CLogger::Info("Construct layout: after branchPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    modpackPage_ = new ModpackPage(this);
    CLogger::Info("Construct layout: after modpackPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    exportTypePage_ = new ExportTypePage(this);
    CLogger::Info("Construct layout: after exportTypePage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    exportDirPage_ = new ExportDirPage(this);
    CLogger::Info("Construct layout: after exportDirPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    extraInfoPage_ = new ExtraInfoPage(this);
    CLogger::Info("Construct layout: after extraInfoPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    buildChecklistPage_ = new BuildChecklistPage(this);
    CLogger::Info("Construct layout: after buildChecklistPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    buildPage_ = new BuildPage(this);
    CLogger::Info("Construct layout: after buildPage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");
    donePage_ = new DonePage(this);
    CLogger::Info("Construct layout: after donePage={}", _CrtCheckMemory() ? "OK" : "CORRUPT");

    stack_->addWidget(repoPage_);
    stack_->addWidget(branchPage_);
    stack_->addWidget(modpackPage_);
    stack_->addWidget(exportTypePage_);
    stack_->addWidget(exportDirPage_);
    stack_->addWidget(extraInfoPage_);
    stack_->addWidget(buildChecklistPage_);
    stack_->addWidget(buildPage_);
    stack_->addWidget(donePage_);

    auto* btnLayout = new QHBoxLayout();
    btnPrev_ = new QPushButton(QString::fromUtf8("\u4e0a\u4e00\u6b65"), this);
    btnNext_ = new QPushButton(QString::fromUtf8("\u4e0b\u4e00\u6b65"), this);
    btnCancel_ = new QPushButton(QString::fromUtf8("\u53d6\u6d88"), this);

    btnPrev_->setMinimumWidth(90);
    btnNext_->setMinimumWidth(90);
    btnCancel_->setMinimumWidth(90);

    btnLayout->addStretch();
    btnLayout->addWidget(btnPrev_);
    btnLayout->addWidget(btnNext_);
    btnLayout->addWidget(btnCancel_);

    mainLayout->addWidget(stack_);
    mainLayout->addLayout(btnLayout);
    setCentralWidget(central);

    buildProgressCard();

    toast_ = nullptr;

    connect(btnPrev_, &QPushButton::clicked, this, &WizardWindow::onPrev);
    connect(btnNext_, &QPushButton::clicked, this, &WizardWindow::onNext);
    connect(btnCancel_, &QPushButton::clicked, this, &WizardWindow::onCancel);

    connect(repoPage_, &RepoPage::repoReady, this, &WizardWindow::onRepoReady);
    connect(branchPage_, &BranchPage::branchSelected, this, &WizardWindow::onBranchSelected);
    connect(branchPage_, &BranchPage::branchesLoaded, this, [this](bool) {
        if (currentPage_ == PAGE_BRANCH) {
            adjustWindowSize(true);
        }
    });
    connect(modpackPage_, &ModpackPage::modpackSelected, this, &WizardWindow::onModpackSelected);
    connect(exportTypePage_, &ExportTypePage::formatSelected, this, &WizardWindow::onExportTypeSelected);
    connect(buildPage_, &BuildPage::buildFinished, this, &WizardWindow::onBuildFinished);
    connect(buildPage_, &BuildPage::progressUpdated, this,
        [this](QString stage, int percent, QString message) {
            if (progressOverlay_ && progressOverlay_->isVisible()) {
                progressMainBar_->setValue(percent);
                progressPercentLabel_->setText(QStringLiteral("%1%").arg(percent));
                progressStatusLabel_->setText(QStringLiteral("[%1] %2").arg(stage, message));
                progressSubLabel_->setText(message);
                QApplication::processEvents();
            }
        });
    connect(buildChecklistPage_, &BuildChecklistPage::treeExpandedChanged, this, [this]() {
        adjustWindowSize(true);
    });
    connect(donePage_, &DonePage::openOutputDirRequested, this, [this](const QString& dir) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    connect(donePage_, &DonePage::finishRequested, this, [this]() {
        close();
    });

    auto* verLabel = new CrashLabel("NSUM v1.0.0", this);
    verLabel->setStyleSheet("color: #999; font-size: 9px; padding-right: 6px;");
    verLabel->installFilter(this);
    statusBar()->addPermanentWidget(verLabel);

auto* helpLabel = new ClickableLabel(QString::fromUtf8("\u5e2e\u52a9\u6587\u6863"), this);
    helpLabel->setToolTip(QString::fromUtf8("\u6253\u5f00\u5f53\u524d\u9875\u9762\u76f8\u5173\u7684\u5e2e\u52a9\u6587\u6863"));
    helpLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    helpLabel->setStyleSheet("color: #4a90d9; font-size: 9px; padding-left: 8px;");
    helpLabel->setCursor(Qt::PointingHandCursor);
    helpLabel->setClicked([this]() { openHelp(); });
    statusBar()->addWidget(helpLabel);

    navigateTo(PAGE_REPO);
    adjustWindowSize(false);
}

WizardWindow::~WizardWindow()
{
    CLogger::Info("Destruct flow: WizardWindow destructor start, heap={}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    CLogger::Info("Destruct flow: WizardWindow destructor end");
}

void WizardWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (progressOverlay_) {
        progressOverlay_->setGeometry(centralWidget()->rect());
    }
    adjustWindowSize(false);
}

void WizardWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (progressOverlay_ && progressOverlay_->isVisible()) {
        progressOverlay_->setGeometry(centralWidget()->rect());
    }
}

void WizardWindow::closeEvent(QCloseEvent* event)
{
    CLogger::Info("Close flow: closeEvent entry heap={}",
        _CrtCheckMemory() ? "OK" : "CORRUPT");
    hideProgressDialog();
    cleanupTempRepo();
    CLogger::Info("Close flow: closeEvent accepted");
    event->accept();
}

void WizardWindow::onNext()
{
    if (flowActive_ && !flowDone_ && flowEndIndex_ == currentPage_) {
        flowFinish();
        return;
    }
    switch (currentPage_) {
    case PAGE_REPO: {
        if (!repoPage_->isValid()) {
            showError(QString::fromUtf8("\u8f93\u5165\u9519\u8bef"),
                QString::fromUtf8("\u8bf7\u8f93\u5165\u6709\u6548\u7684 Git \u4ed3\u5e93\u5730\u5740\u3002"));
            return;
        }
        repoPage_->commitToHistory();
        onRepoReady(repoPage_->repoUrl());
        return;
    }
    case PAGE_BRANCH: {
        QString branch = branchPage_->selectedBranch();
        if (branch.isEmpty()) {
            showError(QString::fromUtf8("\u9009\u62e9\u9519\u8bef"),
                QString::fromUtf8("\u8bf7\u9009\u62e9\u4e00\u4e2a\u5206\u652f\u3002"));
            return;
        }
        onBranchSelected(branch);
        return;
    }
    case PAGE_MODPACK: {
        QString modpack = modpackPage_->selectedModpack();
        if (modpack.isEmpty()) {
            showError(QString::fromUtf8("\u9009\u62e9\u9519\u8bef"),
                QString::fromUtf8("\u8bf7\u9009\u62e9\u4e00\u4e2a\u6574\u5408\u5305\u5206\u652f\u3002"));
            return;
        }
        modpackBranch_ = modpack;
        navigateTo(PAGE_EXPORT_TYPE);
        return;
    }
    case PAGE_EXPORT_TYPE: {
        if (!exportTypePage_->hasSelection()) {
            showError(QString::fromUtf8("\u9009\u62e9\u9519\u8bef"),
                QString::fromUtf8("\u8bf7\u9009\u62e9\u4e00\u79cd\u5bfc\u51fa\u7c7b\u578b\u3002"));
            return;
        }
        navigateTo(PAGE_EXPORT_DIR);
        return;
    }
    case PAGE_EXPORT_DIR: {
        if (!exportDirPage_->isValid()) {
            showError(QString::fromUtf8("\u9009\u62e9\u9519\u8bef"),
                QString::fromUtf8("\u8bf7\u9009\u62e9\u5bfc\u51fa\u76ee\u5f55\u3002"));
            return;
        }
        exportOutputPath_ = exportDirPage_->outputPath();
        CLogger::Info("User selected export dir: {}", exportOutputPath_.toUtf8().constData());

        extraInfoPage_->loadFormat(exportFormat_);
        if (extraInfoPage_->hasFields()) {
            navigateTo(PAGE_EXTRA_INFO);
        } else {
            populateChecklist();
            navigateTo(PAGE_BUILD_CHECKLIST);
        }
        return;
    }
    case PAGE_EXTRA_INFO: {
        QStringList missing = extraInfoPage_->missingRequired();
        if (!missing.isEmpty()) {
            extraInfoPage_->markMissing(missing);
            showError(QString::fromUtf8("\u5fc5\u586b\u9879\u7f3a\u5931"),
                QString::fromUtf8("\u8bf7\u586b\u5199\u5fc5\u586b\u9879:\n%1")
                    .arg(missing.join(QStringLiteral(", "))));
            return;
        }
        CLogger::Info("User submitted extra fields: {} fields", extraInfoPage_->values().size());
        populateChecklist();
        navigateTo(PAGE_BUILD_CHECKLIST);
        return;
    }
    case PAGE_BUILD_CHECKLIST: {
        btnNext_->setEnabled(false);
        navigateTo(PAGE_BUILD);
        buildPage_->startBuild(repoLocalPath_, gitBranch_, modpackBranch_,
            exportFormat_, exportOutputPath_);
        return;
    }
    case PAGE_BUILD: {
        navigateTo(PAGE_DONE);
        return;
    }
    case PAGE_DONE: {
        close();
        return;
    }
    }
}

void WizardWindow::onPrev()
{
    if (flowActive_) {
        if (currentPage_ > flowStartIndex_ && currentPage_ > 0) {
            navigateTo(currentPage_ - 1);
        }
        return;
    }
    if (currentPage_ > 0) {
        navigateTo(currentPage_ - 1);
    }
}

void WizardWindow::onCancel()
{
    if (currentPage_ == PAGE_BUILD) {
        int ret = QMessageBox::question(this,
            QString::fromUtf8("\u786e\u8ba4\u53d6\u6d88"),
            QString::fromUtf8("\u6784\u5efa\u6b63\u5728\u8fdb\u884c\u4e2d\uff0c\u786e\u5b9a\u8981\u53d6\u6d88\u5417\uff1f"),
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            buildPage_->cancelBuild();
            btnNext_->setEnabled(true);
        }
        return;
    }
    close();
}

void WizardWindow::onRepoReady(QString url)
{
    repoUrl_ = url;
    remoteSource_ = isRemoteUrl(url);
    CLogger::Info("User selected repo: type={} url={}",
        remoteSource_ ? "remote" : "local", url.toUtf8().constData());

    // 陌生仓库 (dubious ownership): git 拒绝访问, 询问用户是否信任后继续
    auto ensureRepoTrusted = [this](const QString& dir) -> bool {
        NeoWorkspace::GitOperations gitOps;
        if (gitOps.isGitRepository(dir.toStdString())) return true;
        if (!gitOps.isDubiousOwnership(dir.toStdString())) return true;
        auto reply = QMessageBox::warning(this, "仓库信任",
            QString("检测到该目录是一个未信任的 Git 仓库（可能从其他设备复制而来）:\n%1\n\n"
                    "是否信任该仓库并继续?").arg(dir),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) {
            CLogger::Info("Repo: user declined to trust dubious repo at {}",
                dir.toUtf8().constData());
            return false;
        }
        auto trust = gitOps.trustRepository(dir.toStdString());
        if (trust.exitCode != 0) {
            CLogger::Error("Repo: failed to trust repo at {}: {}",
                dir.toUtf8().constData(), trust.stderrOutput);
            QMessageBox::critical(this, "信任失败",
                QString("无法将仓库加入信任列表:\n%1")
                    .arg(QString::fromStdString(trust.stderrOutput)));
            return false;
        }
        CLogger::Info("Repo: trusted at {}", dir.toUtf8().constData());
        return true;
    };

    if (remoteSource_) {
        startClone(url);
    } else if (repoPage_->sourceType() == RepoPage::SourceCache
        && QDir(url + QStringLiteral("/.git")).exists()) {
        if (!ensureRepoTrusted(url)) return;
        repoLocalPath_ = url;
        CLogger::Info("Using remote repo local cache: {}", url.toUtf8().constData());
        startSyncCache(url);
    } else {
        if (!ensureRepoTrusted(url)) return;
        setProgressTitle(QString::fromUtf8("\u89e3\u6790\u4ed3\u5e93"));
        showProgressDialog(QString::fromUtf8("\u6b63\u5728\u89e3\u6790\u4ed3\u5e93..."), true, false);
        repoLocalPath_ = url;
        branchPage_->loadBranches(repoLocalPath_);
        hideProgressDialog();
        if (!(flowActive_ && (flowDone_ || currentPage_ > PAGE_BRANCH))) {
            navigateTo(PAGE_BRANCH);
        }
    }
}

void WizardWindow::startSyncCache(const QString& dir)
{
    setProgressTitle(QString::fromUtf8("\u540c\u6b65\u8fdc\u7a0b\u4ed3\u5e93"));
    showProgressDialog(QString::fromUtf8("\u6b63\u5728\u540c\u6b65\u8fdc\u7a0b\u4ed3\u5e93..."), true, true);
    progressSubLabel_->setText(QString::fromUtf8("\u6b63\u5728\u62c9\u53d6\u8fdc\u7a0b\u66f4\u65b0..."));
    syncProcess_ = new QProcess(this);
    syncProcess_->setWorkingDirectory(dir);
    connect(syncProcess_,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this](int exitCode, QProcess::ExitStatus status) {
            if (syncProcess_) {
                syncProcess_->deleteLater();
                syncProcess_ = nullptr;
            }
            if (status == QProcess::CrashExit || exitCode != 0) {
                progressStatusLabel_->setText(QString::fromUtf8("\u540c\u6b65\u5931\u8d25\uff0c\u4f7f\u7528\u7f13\u5b58\u5185\u5bb9"));
            }
            finishRepoLoading();
        });
    connect(syncProcess_, &QProcess::readyReadStandardError, this, [this]() {
        if (syncProcess_) {
            QString line = QString::fromUtf8(syncProcess_->readAllStandardError()).trimmed();
            parseGitProgress(line, true);
        }
    });
    syncProcess_->start(QStringLiteral("git"),
        { QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune") });
}

void WizardWindow::onCloneFinished(int exitCode, QProcess::ExitStatus status)
{
    QString err;
    if (cloneProcess_) {
        err = QString::fromUtf8(cloneProcess_->readAllStandardError());
        cloneProcess_->deleteLater();
        cloneProcess_ = nullptr;
    }

    if (status == QProcess::CrashExit || exitCode != 0) {
        hideProgressDialog();
        CLogger::Error("Clone failed: exit={} {}", exitCode, err.left(200).toUtf8().constData());
        branchPage_->showLoading(
            QStringLiteral("\u514b\u9686\u5931\u8d25: %1").arg(err.left(120)), -1);
        branchPage_->stopLoading();
        emit branchPage_->branchesLoaded(false);
        return;
    }

    CLogger::Info("Clone succeeded: {}", repoLocalPath_.toUtf8().constData());
    finishRepoLoading();
}

void WizardWindow::onBranchSelected(QString branch)
{
    gitBranch_ = branch;
    CLogger::Info("User selected Git branch: {}", branch.toUtf8().constData());
    if (flowActive_ && flowEndIndex_ == PAGE_BRANCH) {
        flowFinish();
        return;
    }
    setProgressTitle(QString::fromUtf8("\u89e3\u6790\u6574\u5408\u5305\u914d\u7f6e"));
    showProgressDialog(QString::fromUtf8("\u6b63\u5728\u89e3\u6790\u6574\u5408\u5305\u914d\u7f6e..."), true, false);
    modpackPage_->loadModpacks(repoLocalPath_);
    hideProgressDialog();
    if (!(flowActive_ && (flowDone_ || currentPage_ > PAGE_MODPACK))) {
        navigateTo(PAGE_MODPACK);
    }
}

void WizardWindow::onModpackSelected(QString modpack)
{
    modpackBranch_ = modpack;
    CLogger::Info("User selected modpack branch: {}", modpack.toUtf8().constData());
    if (flowActive_ && flowEndIndex_ == PAGE_MODPACK) {
        flowFinish();
        return;
    }
    navigateTo(PAGE_EXPORT_TYPE);
    if (flowActive_) {
        flowMaybeAdvanceFrom(PAGE_EXPORT_TYPE);
    }
}

void WizardWindow::onExportTypeSelected(QString format)
{
    exportFormat_ = format;
    CLogger::Info("User selected export format: {}", format.toUtf8().constData());
    QString ext = (format == QLatin1String("modrinth")) ? QStringLiteral(".mrpack") : QStringLiteral(".zip");
    if (format == QLatin1String("hmcl")) {
        ext = QStringLiteral("");
    }
    exportDirPage_->setContext(modpackBranch_, exportFormat_, ext);
    if (flowActive_ && flowEndIndex_ == PAGE_EXPORT_TYPE) {
        flowFinish();
        return;
    }
    navigateTo(PAGE_EXPORT_DIR);
    if (flowActive_) {
        flowMaybeAdvanceFrom(PAGE_EXPORT_DIR);
    }
}

void WizardWindow::populateChecklist()
{
    buildChecklistPage_->clearSummary();
    buildChecklistPage_->setSummary(QString::fromUtf8("\u4ed3\u5e93"),
        remoteSource_ ? repoUrl_ : repoLocalPath_);
    buildChecklistPage_->setSummary(QString::fromUtf8("Git \u5206\u652f"), gitBranch_);
    buildChecklistPage_->setSummary(QString::fromUtf8("\u6574\u5408\u5305\u5206\u652f"), modpackBranch_);
    buildChecklistPage_->setSummary(QString::fromUtf8("\u5bfc\u51fa\u683c\u5f0f"),
        exportTypePage_->selectedFormatName());
    buildChecklistPage_->setSummary(QString::fromUtf8("\u5bfc\u51fa\u76ee\u5f55"), exportOutputPath_);

    QMap<QString, QString> extra;
    if (extraInfoPage_->hasFields()) {
        extra = extraInfoPage_->values();
    }
    buildChecklistPage_->setExtraInfo(extra);

    CLogger::Info("Checklist: generating virtual build preview repo={} branch={} format={}",
        repoLocalPath_.toUtf8().constData(), modpackBranch_.toUtf8().constData(),
        exportFormat_.toUtf8().constData());

    // 虚拟构建工作区 = 用户真实仓库 (workspace.json 自然存在), 与真实构建一致;
    // cache/output 落系统临时目录 (预览产物一次性, 不污染仓库与发布目录)
    QString tmpBuildRoot = QDir::tempPath()
        + QStringLiteral("/NSUM-virtual-build/") + modpackBranch_;
    QString cacheDir = tmpBuildRoot + QStringLiteral("/.cache");
    QString outputDir = tmpBuildRoot + QStringLiteral("/output");
    QString exportersDir = QCoreApplication::applicationDirPath()
        + QStringLiteral("/exporters");

    NeoBuild::BuildEngine engine;
    if (!engine.init(repoLocalPath_.toStdString(), cacheDir.toStdString(),
            outputDir.toStdString(), exportersDir.toStdString())) {
        CLogger::Error("Checklist: virtual build init failed (workspace={})",
            repoLocalPath_.toUtf8().constData());
        buildChecklistPage_->clearFileTree();
        showError(QString::fromUtf8("\u9884\u89c8\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u521d\u59cb\u5316\u6784\u5efa\u5f15\u64ce\uff0c\u8bf7\u68c0\u67e5\u4ed3\u5e93\u8def\u5f84\u3002"));
        return;
    }

    NeoCore::ExportMetadata meta;
    meta.name = modpackBranch_.toStdString();
    auto extraMap = extra;
    for (auto it = extraMap.constBegin(); it != extraMap.constEnd(); ++it) {
        meta.extra[it.key().toStdString()] = it.value().toStdString();
    }

    // 执行虚拟构建: 生成 build_dir 产物 (UMD 对比基础); checkout 用 git 分支, merge 用整合包分支
    NeoCore::BuildResult vr = engine.build(modpackBranch_.toStdString(), nullptr, nullptr,
        gitBranch_.toStdString());
    if (!vr.success) {
        CLogger::Error("Checklist: virtual build failed: {}",
            vr.errorMessage.empty() ? "unknown" : vr.errorMessage);
        buildChecklistPage_->clearFileTree();
        showError(QString::fromUtf8("\u9884\u89c8\u5931\u8d25"),
            QString::fromUtf8("\u864a\u62df\u6784\u5efa\u5931\u8d25: %1")
                .arg(QString::fromUtf8(vr.errorMessage.c_str())));
        return;
    }

    QString targetDir;
    if (exportFormat_ == QLatin1String("hmcl")) {
        targetDir = exportOutputPath_;
    }
    nlohmann::json structure = engine.previewStructure(exportFormat_.toStdString(), meta,
        targetDir.toStdString());
    CLogger::Info("Checklist: preview generated {} entries",
        structure.size());
    buildChecklistPage_->loadStructure(structure);
}

void WizardWindow::showError(const QString& title, const QString& detail)
{
    if (!toast_) {
        toast_ = new ToastNotification(this);
        connect(toast_, &QObject::destroyed, this, [this]() {
            toast_ = nullptr;
        });
    }
    toast_->showError(title, detail);
}

void WizardWindow::onBuildFinished(bool success, QString message, QStringList warnings)
{
    lastBuildFailed_ = !success;
    if (success) {
        cleanupTempRepo();
        buildDir_ = message;
        btnNext_->setEnabled(true);
        donePage_->showSuccess(buildDir_, warnings);
        navigateTo(PAGE_DONE);
        CLogger::Info("Build succeeded: {}", message.toUtf8().constData());
    } else {
        donePage_->showFailure(message,
            QString::fromUtf8("\u8bf7\u68c0\u67e5\u4ed3\u5e93\u8def\u5f84\u3001Git \u73af\u5883\u548c\u8f93\u51fa\u76ee\u5f55\u6743\u9650\uff0c\u7136\u540e\u91cd\u65b0\u5c1d\u8bd5\u3002"));
        navigateTo(PAGE_DONE);
        CLogger::Error("Build failed: {}", message.toUtf8().constData());
    }
}

void WizardWindow::openHelp()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString docsRoot = exeDir + QStringLiteral("/docs");
    QString file;
    QString anchor;

    if (flowActive_) {
        file = docsRoot + QStringLiteral("/CLI/CLI-flow.md");
    } else {
        switch (currentPage_) {
        case PAGE_REPO:         anchor = QString::fromUtf8("\u4ed3\u5e93\u9009\u62e9"); break;
        case PAGE_BRANCH:       anchor = QString::fromUtf8("\u5206\u652f\u9009\u62e9"); break;
        case PAGE_MODPACK:      anchor = QString::fromUtf8("\u6574\u5408\u5305\u9009\u62e9"); break;
        case PAGE_EXPORT_TYPE:  anchor = QString::fromUtf8("\u5bfc\u51fa\u7c7b\u578b"); break;
        case PAGE_EXPORT_DIR:   anchor = QString::fromUtf8("\u5bfc\u51fa\u76ee\u5f55"); break;
        case PAGE_EXTRA_INFO:   anchor = QString::fromUtf8("\u989d\u5916\u4fe1\u606f"); break;
        case PAGE_BUILD_CHECKLIST: anchor = QString::fromUtf8("\u6784\u5efa\u6e05\u5355"); break;
        case PAGE_BUILD:        anchor = QString::fromUtf8("\u6784\u5efa\u6267\u884c"); break;
        case PAGE_DONE:
            if (lastBuildFailed_) {
                file = docsRoot + QStringLiteral("/main/troubleshooting.md");
                anchor = QString::fromUtf8("\u6784\u5efa\u5931\u8d25");
            } else {
                anchor = QString::fromUtf8("\u6784\u5efa\u5b8c\u6210");
            }
            break;
        default:
            break;
        }
        if (file.isEmpty())
            file = docsRoot + QStringLiteral("/main/operation-guide.md");
    }

    QStringList args;
    if (!anchor.isEmpty())
        args << QStringLiteral("--anchor") << anchor;
    if (!PowerHelper::Bridge::launchReader(file, args)) {
        // 目标文件缺失时退回文档组
        if (!PowerHelper::Bridge::launchReader(docsRoot)) {
            QMessageBox::warning(this,
                QString::fromUtf8("\u65e0\u6cd5\u6253\u5f00\u5e2e\u52a9"),
                QString::fromUtf8("\u672a\u627e\u5230 PowerHelper.exe\uff0c\u65e0\u6cd5\u6253\u5f00\u5e2e\u52a9\u6587\u6863\u3002"));
        }
    }
}

bool WizardWindow::isRemoteUrl(const QString& url) const
{
    return url.startsWith(QLatin1String("https://"))
        || url.startsWith(QLatin1String("http://"))
        || url.startsWith(QLatin1String("git@"))
        || url.startsWith(QLatin1String("ssh://"));
}

void WizardWindow::startClone(const QString& url)
{
    cleanupTempRepo();

    QString cacheRoot = QCoreApplication::applicationDirPath()
        + QStringLiteral("/config/history/cache");
    QDir().mkpath(cacheRoot);

    QByteArray hash = QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex();
    QString cacheDir = cacheRoot + QStringLiteral("/") + QString::fromLatin1(hash.left(16));

    repoLocalPath_ = cacheDir;
    CLogger::Info("Clone remote repo start: url={} cache={}", url.toUtf8().constData(),
        cacheDir.toUtf8().constData());

    if (QDir(cacheDir + QStringLiteral("/.git")).exists()) {
        CLogger::Info("Cache exists, skip clone, sync directly: {}", cacheDir.toUtf8().constData());
        navigateTo(PAGE_BRANCH);
        startSyncCache(cacheDir);
        return;
    }

    navigateTo(PAGE_BRANCH);
    setProgressTitle(QString::fromUtf8("\u514b\u9686\u8fdc\u7a0b\u4ed3\u5e93"));
    showProgressDialog(QString::fromUtf8("\u6b63\u5728\u514b\u9686\u8fdc\u7a0b\u4ed3\u5e93..."), true, true);
    progressSubLabel_->setText(QString::fromUtf8("\u6b63\u5728\u4e0b\u8f7d\u5386\u53f2\u6570\u636e..."));

    cloneProcess_ = new QProcess(this);
    cloneProcess_->setWorkingDirectory(cacheRoot);
    connect(cloneProcess_,
        QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &WizardWindow::onCloneFinished);
    connect(cloneProcess_, &QProcess::readyReadStandardOutput, this, [this]() {
        if (cloneProcess_) {
            QString line = QString::fromUtf8(cloneProcess_->readAllStandardOutput()).trimmed();
            if (!line.isEmpty()) {
                progressStatusLabel_->setText(QString::fromUtf8("\u6b63\u5728\u514b\u9686: %1").arg(line));
            }
        }
    });
    connect(cloneProcess_, &QProcess::readyReadStandardError, this, [this]() {
        if (cloneProcess_) {
            QString line = QString::fromUtf8(cloneProcess_->readAllStandardError()).trimmed();
            parseGitProgress(line, false);
        }
    });
    cloneProcess_->start(QStringLiteral("git"),
        { QStringLiteral("clone"),
          QStringLiteral("--no-single-branch"),
          url,
          cacheDir });
}

void WizardWindow::buildProgressCard()
{
    QWidget* central = centralWidget();
    if (!central) return;

    progressOverlay_ = new QWidget(central);
    progressOverlay_->setObjectName(QStringLiteral("progressOverlay"));
    progressOverlay_->setGeometry(central->rect());

    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    progressOverlay_->setStyleSheet(QStringLiteral(
        "#progressOverlay { background: rgba(0, 0, 0, %1); }")
        .arg(darkMode ? 90 : 40));

    auto* overlayLayout = new QVBoxLayout(progressOverlay_);
    overlayLayout->setContentsMargins(0, 0, 0, 0);

    progressCard_ = new QFrame(progressOverlay_);
    progressCard_->setObjectName(QStringLiteral("progressCard"));
    progressCard_->setFixedWidth(420);
    progressCard_->setStyleSheet(QStringLiteral(
        "QFrame#progressCard {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}")
        .arg(palette().color(QPalette::Base).name(),
             darkMode ? QStringLiteral("#55585e") : QStringLiteral("#d0d0d0")));

    auto* cardLayout = new QVBoxLayout(progressCard_);
    cardLayout->setContentsMargins(24, 20, 24, 20);
    cardLayout->setSpacing(10);

    progressTitleLabel_ = new QLabel(QString::fromUtf8("\u52a0\u8f7d\u4e2d"), progressCard_);
    QFont titleFont = progressTitleLabel_->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    progressTitleLabel_->setFont(titleFont);
    progressTitleLabel_->setStyleSheet(QStringLiteral("color: %1; border: none;")
        .arg(darkMode ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000")));
    cardLayout->addWidget(progressTitleLabel_);

    progressStatusLabel_ = new QLabel(progressCard_);
    progressStatusLabel_->setWordWrap(true);
    progressStatusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; border: none;")
        .arg(darkMode ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    cardLayout->addWidget(progressStatusLabel_);

    auto* mainRow = new QHBoxLayout();
    mainRow->setSpacing(8);
    progressMainBar_ = new QProgressBar(progressCard_);
    progressMainBar_->setRange(0, 0);
    progressMainBar_->setTextVisible(false);
    progressMainBar_->setFixedHeight(8);
    mainRow->addWidget(progressMainBar_, 1);
    progressPercentLabel_ = new QLabel(QStringLiteral("..."), progressCard_);
    progressPercentLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px; border: none;")
        .arg(darkMode ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    mainRow->addWidget(progressPercentLabel_);
    cardLayout->addLayout(mainRow);

    progressSubBar_ = new QProgressBar(progressCard_);
    progressSubBar_->setRange(0, 100);
    progressSubBar_->setValue(0);
    progressSubBar_->setFixedHeight(6);
    progressSubBar_->setTextVisible(false);
    progressSubBar_->hide();
    cardLayout->addWidget(progressSubBar_);

    progressSubLabel_ = new QLabel(progressCard_);
    progressSubLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 11px; border: none;")
        .arg(darkMode ? QStringLiteral("#7a7f88") : QStringLiteral("#888")));
    progressSubLabel_->hide();
    cardLayout->addWidget(progressSubLabel_);

    overlayLayout->addStretch(1);
    auto* centerRow = new QHBoxLayout();
    centerRow->addStretch(1);
    centerRow->addWidget(progressCard_);
    centerRow->addStretch(1);
    overlayLayout->addLayout(centerRow);
    overlayLayout->addStretch(1);

    progressOverlay_->hide();
    progressCard_->raise();
}

void WizardWindow::setProgressTitle(const QString& title)
{
    if (progressTitleLabel_) {
        progressTitleLabel_->setText(title);
    }
}

void WizardWindow::showProgressDialog(const QString& text, bool indeterminate, bool withSub)
{
    if (!progressOverlay_) return;
    progressStatusLabel_->setText(text);
    progressMainBar_->setRange(0, indeterminate ? 0 : 100);
    progressMainBar_->setTextVisible(false);
    progressPercentLabel_->setText(indeterminate ? QStringLiteral("...") : QStringLiteral("0%"));
    progressSubBar_->setVisible(withSub);
    progressSubLabel_->setVisible(withSub);
    if (withSub) {
        progressSubBar_->setValue(0);
        progressSubLabel_->setText(QString());
    }
    progressOverlay_->setGeometry(centralWidget()->rect());
    progressOverlay_->show();
    progressOverlay_->raise();
    QApplication::processEvents();
}

void WizardWindow::hideProgressDialog()
{
    if (progressOverlay_) {
        progressOverlay_->hide();
    }
    QApplication::processEvents();
}

void WizardWindow::parseGitProgress(const QString& line, bool isFetch)
{
    if (!progressOverlay_ || !progressOverlay_->isVisible()) return;

    QRegularExpression re(QStringLiteral("(\\d+)%"));
    auto match = re.match(line);
    if (match.hasMatch()) {
        int percent = match.captured(1).toInt();
        progressSubBar_->setValue(percent);
        progressPercentLabel_->setText(QStringLiteral("%1%").arg(percent));
        if (isFetch) {
            progressSubLabel_->setText(QString::fromUtf8("\u540c\u6b65\u4e2d: %1%").arg(percent));
        } else {
            progressSubLabel_->setText(QString::fromUtf8("\u4e0b\u8f7d\u4e2d: %1%").arg(percent));
        }
    }
}

void WizardWindow::finishRepoLoading()
{
    hideProgressDialog();
    branchPage_->loadBranches(repoLocalPath_);
    if (!(flowActive_ && (flowDone_ || currentPage_ > PAGE_BRANCH))) {
        navigateTo(PAGE_BRANCH);
    }
    if (remoteSource_) {
        repoPage_->recordRecentCache(repoUrl_, repoLocalPath_);
    }
}

void WizardWindow::cleanupTempRepo()
{
    if (cloneProcess_) {
        cloneProcess_->kill();
        cloneProcess_->waitForFinished(3000);
        cloneProcess_->deleteLater();
        cloneProcess_ = nullptr;
    }
    if (syncProcess_) {
        syncProcess_->kill();
        syncProcess_->waitForFinished(3000);
        syncProcess_->deleteLater();
        syncProcess_ = nullptr;
    }
}

void WizardWindow::navigateTo(int pageIndex)
{
    currentPage_ = pageIndex;
    stack_->setCurrentIndex(pageIndex);
    updateButtons();
    adjustWindowSize(pageIndex != PAGE_BUILD);
    fadeTransition();
    CLogger::Info("Page navigation: {} -> {}", indexToPageName(pageIndex).toUtf8().constData(),
        indexToPageName(currentPage_).toUtf8().constData());
}

void WizardWindow::updateButtons()
{
    if (flowActive_) {
        btnPrev_->setEnabled(currentPage_ > flowStartIndex_
            && currentPage_ != PAGE_BUILD && currentPage_ != PAGE_DONE);
        if (currentPage_ == flowEndIndex_) {
            btnNext_->setText(QString::fromUtf8("\u5b8c\u6210"));
            btnCancel_->setText(QString::fromUtf8("\u53d6\u6d88"));
        } else if (currentPage_ == PAGE_BUILD) {
            btnNext_->setText(QString::fromUtf8("\u4e0b\u4e00\u6b65"));
            btnNext_->setEnabled(false);
            btnCancel_->setText(QString::fromUtf8("\u53d6\u6d88"));
        } else {
            btnNext_->setText(QString::fromUtf8("\u4e0b\u4e00\u6b65"));
            btnCancel_->setText(QString::fromUtf8("\u53d6\u6d88"));
        }
        return;
    }

    btnPrev_->setEnabled(currentPage_ != PAGE_REPO
        && currentPage_ != PAGE_BUILD
        && currentPage_ != PAGE_DONE);

    if (currentPage_ == PAGE_DONE) {
        btnNext_->setText(QString::fromUtf8("\u5b8c\u6210"));
        btnCancel_->setText(QString::fromUtf8("\u5173\u95ed"));
    } else if (currentPage_ == PAGE_BUILD_CHECKLIST) {
        btnNext_->setText(QString::fromUtf8("\u5f00\u59cb\u6784\u5efa"));
        btnCancel_->setText(QString::fromUtf8("\u53d6\u6d88"));
    } else {
        btnNext_->setText(QString::fromUtf8("\u4e0b\u4e00\u6b65"));
        btnCancel_->setText(QString::fromUtf8("\u53d6\u6d88"));
    }

    if (currentPage_ == PAGE_BUILD) {
        btnNext_->setEnabled(false);
    } else if (currentPage_ == PAGE_REPO) {
        btnNext_->setEnabled(true);
    }
}

void WizardWindow::adjustWindowSize(bool animate)
{
    QSize full = stack_->currentWidget()->sizeHint();

    int widthHint = qMax(full.width(), 640) - 48;
    if (currentPage_ == PAGE_BRANCH) {
        int contentH = branchPage_->contentHeight(widthHint);
        if (contentH > 0) {
            full.setHeight(contentH);
        }
    } else if (currentPage_ == PAGE_MODPACK) {
        int contentH = modpackPage_->contentHeight(widthHint);
        if (contentH > 0) {
            full.setHeight(contentH);
        }
    } else if (currentPage_ == PAGE_BUILD_CHECKLIST) {
        int contentH = buildChecklistPage_->contentHeight(widthHint);
        if (contentH > 0) {
            full.setHeight(contentH);
        }
    } else if (currentPage_ == PAGE_EXTRA_INFO) {
        int contentH = extraInfoPage_->contentHeight(widthHint);
        if (contentH > 0) {
            full.setHeight(contentH);
        }
    }

    const int btnH = qMax(btnPrev_->sizeHint().height(), 28);
    const int oldPad = 90 - btnH;
    const int newPad = qMax(6, oldPad * 3 / 10);
    const int overhead = btnH + newPad;

    QSize target(full.width() + 48, full.height() + overhead);

    const int minW = 640, minH = 100;
    target.setWidth(std::max(target.width(), minW));
    target.setHeight(std::max(target.height(), minH));

    QRect avail = screen() ? screen()->availableGeometry() : QRect(0, 0, 1280, 800);
    const int maxH = static_cast<int>(avail.height() * 0.80f);
    const int maxW = static_cast<int>(avail.width() * 0.80f);
    target = target.boundedTo(QSize(maxW, maxH));
    if (target.width() < minW) target.setWidth(minW);
    if (target.height() < minH) target.setHeight(minH);

    QPoint center = avail.center();
    if (positioned_) {
        QRect cur = geometry();
        if (cur.isValid()) {
            center = cur.center();
        }
    }
    QRect newGeometry(center.x() - target.width() / 2,
                 center.y() - target.height() / 2,
                 target.width(), target.height());
    if (newGeometry.right() > avail.right()) newGeometry.moveRight(avail.right());
    if (newGeometry.left() < avail.left()) newGeometry.moveLeft(avail.left());
    if (newGeometry.bottom() > avail.bottom()) newGeometry.moveBottom(avail.bottom());
    if (newGeometry.top() < avail.top()) newGeometry.moveTop(avail.top());
    positioned_ = true;

    if (animate && isVisible()) {
        QRect cur = geometry();
        bool sameSize = (cur.size() == newGeometry.size());
        bool samePos = (cur.topLeft() == newGeometry.topLeft());
        if (sameSize && samePos) {
            return;
        }
        setMaximumSize(16777215, 16777215);
        setMinimumSize(0, 0);
        auto* anim = new QPropertyAnimation(this, "geometry", this);
        anim->setDuration(200);
        QEasingCurve easing(QEasingCurve::OutCubic);
        anim->setEasingCurve(easing);
        anim->setStartValue(cur);
        anim->setEndValue(newGeometry);
        QRect endGeo = newGeometry;
        connect(anim, &QPropertyAnimation::finished, this, [this, endGeo]() {
            setGeometry(endGeo);
            setFixedSize(endGeo.size());
        });
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        setGeometry(newGeometry);
        setFixedSize(newGeometry.size());
    }
}

void WizardWindow::fadeTransition()
{
    QWidget* page = stack_->currentWidget();
    if (!page) return;

    if (page->graphicsEffect()) {
        page->setGraphicsEffect(nullptr);
    }

    auto* effect = new QGraphicsOpacityEffect(page);
    page->setGraphicsEffect(effect);
    auto* anim = new QPropertyAnimation(effect, "opacity", page);
    anim->setDuration(160);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    QObject::connect(anim, &QPropertyAnimation::finished, page, [page]() {
        page->setGraphicsEffect(nullptr);
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void WizardWindow::handlePageActivation(int page)
{
    // 构建由 onNext 中 PAGE_BUILD_CHECKLIST 分支显式启动，
    // 此处不再重复触发（真实构建为后台线程，重复启动会并发构建）。
    Q_UNUSED(page)
}

int WizardWindow::pageNameToIndex(const QString& name)
{
    const QString n = name.trimmed();
    if (n == QLatin1String("repo"))        return 0;
    if (n == QLatin1String("branch"))      return 1;
    if (n == QLatin1String("modpack"))     return 2;
    if (n == QLatin1String("export-type")) return 3;
    if (n == QLatin1String("export-dir"))  return 4;
    if (n == QLatin1String("extra-info"))  return 5;
    if (n == QLatin1String("checklist"))   return 6;
    if (n == QLatin1String("build"))       return 7;
    if (n == QLatin1String("done"))        return 8;
    return -1;
}

QString WizardWindow::indexToPageName(int index)
{
    switch (index) {
    case 0: return QStringLiteral("repo");
    case 1: return QStringLiteral("branch");
    case 2: return QStringLiteral("modpack");
    case 3: return QStringLiteral("export-type");
    case 4: return QStringLiteral("export-dir");
    case 5: return QStringLiteral("extra-info");
    case 6: return QStringLiteral("checklist");
    case 7: return QStringLiteral("build");
    case 8: return QStringLiteral("done");
    default: return QString();
    }
}

void WizardWindow::setFlowMode(const FlowConfig& cfg)
{
    flowCfg_ = cfg;
    flowActive_ = true;
    flowDone_ = false;

    int end = -1;
    if (!flowCfg_.endPage.isEmpty()) {
        end = pageNameToIndex(flowCfg_.endPage);
    }
    if (end < 0) end = PAGE_DONE;
    if (flowCfg_.collectOnly && end > PAGE_BUILD_CHECKLIST) {
        end = PAGE_BUILD_CHECKLIST;
    }
    flowEndIndex_ = end;

    connect(branchPage_, &BranchPage::branchesLoaded, this, [this](bool ok) {
        if (!flowActive_ || flowDone_ || !ok) return;
        if (currentPage_ > PAGE_BRANCH) return;
        if (flowEndIndex_ < PAGE_BRANCH) return;
        if (flowCfg_.prefill.contains("branch"))
            branchPage_->selectBranch(flowCfg_.prefill["branch"]);
    });
    connect(modpackPage_, &ModpackPage::modpacksLoaded, this, [this](bool ok) {
        if (!flowActive_ || flowDone_ || !ok) return;
        if (currentPage_ > PAGE_MODPACK) return;
        if (flowEndIndex_ < PAGE_MODPACK) return;
        if (flowCfg_.prefill.contains("modpack"))
            modpackPage_->selectModpack(flowCfg_.prefill["modpack"]);
    });

    flowInit();
    if (flowActive_ && !flowDone_ && currentPage_ == flowEndIndex_) {
        flowFinish();
    }
}

void WizardWindow::flowTriggerNext()
{
    if (flowActive_ && !flowDone_ && flowEndIndex_ == currentPage_) {
        flowFinish();
    }
}

// Start page = explicit --from, else the first page not satisfied by prefill.
void WizardWindow::flowInit()
{
    if (!flowActive_) return;

    int s = -1;
    if (!flowCfg_.startPage.isEmpty()) {
        s = pageNameToIndex(flowCfg_.startPage);
    }
    if (s < 0) {
        for (int p = PAGE_REPO; p <= flowEndIndex_; ++p) {
            bool satisfied = false;
            switch (p) {
            case PAGE_REPO:        satisfied = flowCfg_.prefill.contains("repo"); break;
            case PAGE_BRANCH:      satisfied = flowCfg_.prefill.contains("branch"); break;
            case PAGE_MODPACK:     satisfied = flowCfg_.prefill.contains("modpack"); break;
            case PAGE_EXPORT_TYPE: satisfied = flowCfg_.prefill.contains("format"); break;
            case PAGE_EXPORT_DIR:  satisfied = flowCfg_.prefill.contains("exportdir"); break;
            default:               satisfied = false; break;
            }
            if (!satisfied) { s = p; break; }
        }
        if (s < 0) s = flowEndIndex_;
    }
    if (s < 0) s = PAGE_REPO;
    if (s > flowEndIndex_) s = flowEndIndex_;
    flowStartIndex_ = s;

    if (flowCfg_.prefill.contains("repo")) {
        QString repo = flowCfg_.prefill["repo"];
        if (repo.startsWith(QLatin1String("file://"))) {
            repo = QUrl(repo).toLocalFile();
        }
        repoPage_->setUrl(repo);
        onRepoReady(repo);
        return;
    }
    navigateTo(s);
}

// Prefill-driven auto-advance after a page has been populated/selected.
// Auto-advance stops at the first page without a prefill value.
void WizardWindow::flowMaybeAdvanceFrom(int page)
{
    if (!flowActive_ || flowDone_) return;
    if (page > flowEndIndex_) return;

    switch (page) {
    case PAGE_EXPORT_TYPE:
        if (flowCfg_.prefill.contains("format"))
            exportTypePage_->selectFormat(flowCfg_.prefill["format"]);
        break;
    case PAGE_EXPORT_DIR:
        if (!flowCfg_.prefill.contains("exportdir")) break;
        exportDirPage_->setPath(flowCfg_.prefill["exportdir"]);
        exportOutputPath_ = exportDirPage_->outputPath();
        if (flowEndIndex_ == PAGE_EXPORT_DIR) {
            flowFinish();
            return;
        }
        extraInfoPage_->loadFormat(exportFormat_);
        if (extraInfoPage_->hasFields()) {
            navigateTo(PAGE_EXTRA_INFO);
            flowMaybeAdvanceFrom(PAGE_EXTRA_INFO);
        } else {
            populateChecklist();
            navigateTo(PAGE_BUILD_CHECKLIST);
        }
        break;
    case PAGE_EXTRA_INFO: {
        for (auto it = flowCfg_.prefill.constBegin();
             it != flowCfg_.prefill.constEnd(); ++it) {
            extraInfoPage_->setValue(it.key(), it.value());
        }
        if (extraInfoPage_->missingRequired().isEmpty()) {
            populateChecklist();
            navigateTo(PAGE_BUILD_CHECKLIST);
        }
        break;
    }
    default:
        break;
    }
}

QString WizardWindow::flowCollectJson() const
{
    nlohmann::json obj;
    obj["category"] = "flow";
    obj["command"] = "gui";

    nlohmann::json data;
    data["repo"] = (repoUrl_.isEmpty() ? repoPage_->repoUrl() : repoUrl_).toStdString();
    data["repo_local_path"] = repoLocalPath_.toStdString();
    data["branch"] = gitBranch_.toStdString();
    data["modpack"] = modpackBranch_.toStdString();
    data["format"] = exportFormat_.toStdString();
    data["export_dir"] = exportOutputPath_.toStdString();
    if (extraInfoPage_->hasFields()) {
        nlohmann::json extra = nlohmann::json::object();
        QMap<QString, QString> vals = extraInfoPage_->values();
        for (auto it = vals.constBegin(); it != vals.constEnd(); ++it) {
            extra[it.key().toStdString()] = it.value().toStdString();
        }
        data["extra"] = extra;
    }
    obj["data"] = data;
    return QString::fromStdString(obj.dump(2));
}

void WizardWindow::flowFinish()
{
    if (!flowActive_ || flowDone_) return;
    flowDone_ = true;
    hideProgressDialog();
    emit flowDataReady(flowCollectJson());
    close();
}

} // namespace GUIWorker

#include "wizard_window.moc"


