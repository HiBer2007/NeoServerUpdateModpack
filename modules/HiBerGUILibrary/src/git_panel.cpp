#include "git_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QInputDialog>
#include <QProcess>
#include <QDir>
#include <QFont>
#include <QTimer>
#include <QDebug>
#include <algorithm>
#include <crtdbg.h>

namespace HiBerGUI {

GitPanel::GitPanel(QWidget* parent)
    : QWidget(parent), refreshTimer_(nullptr)
{
    setMinimumWidth(260);
    buildUI();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(5000);
    connect(refreshTimer_, &QTimer::timeout, this, &GitPanel::onRefresh);
    refreshTimer_->start();
}

GitPanel::~GitPanel()
{
    qInfo() << "Destruct flow: GitPanel destructor start, heap="
        << (_CrtCheckMemory() ? "OK" : "CORRUPT");
    if (refreshTimer_) {
        refreshTimer_->stop();
        disconnect(refreshTimer_, &QTimer::timeout, this, &GitPanel::onRefresh);
    }
    qInfo() << "Destruct flow: GitPanel destructor end";
}

void GitPanel::buildUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(2);

    branchLabel_ = new QLabel("未打开仓库", this);
    branchLabel_->setStyleSheet("color: #007acc; font-weight: bold; font-size: 10px;");
    branchLabel_->setWordWrap(true);

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("font-size: 10px;");

    layout->addWidget(branchLabel_);
    layout->addWidget(statusLabel_);

    auto* vSplit = new QSplitter(Qt::Vertical, this);

    historyTree_ = new QTreeWidget(vSplit);
    historyTree_->setHeaderLabels({"历史记录"});
    historyTree_->header()->hide();
    historyTree_->setRootIsDecorated(false);
    historyTree_->setIndentation(10);
    historyTree_->setAlternatingRowColors(true);
    historyTree_->setFont(QFont("Consolas", 8));
    historyTree_->setMinimumHeight(60);

    auto* changesWidget = new QWidget(vSplit);
    auto* changesLayout = new QVBoxLayout(changesWidget);
    changesLayout->setContentsMargins(0, 4, 0, 0);
    changesLayout->setSpacing(2);

    stagedHeader_ = new QLabel("暂存", changesWidget);
    stagedHeader_->setStyleSheet("font-weight: bold; font-size: 10px;");
    stagedTree_ = new QTreeWidget(changesWidget);
    stagedTree_->setHeaderLabels({"文件", ""});
    stagedTree_->setRootIsDecorated(false);
    stagedTree_->header()->hide();
    stagedTree_->header()->setStretchLastSection(true);
    stagedTree_->setMaximumHeight(80);
    stagedTree_->setAlternatingRowColors(true);
    stagedTree_->setFont(QFont("Consolas", 8));

    unstageBtn_ = new QPushButton("  -  取消暂存", changesWidget);
    unstageBtn_->setMinimumHeight(28);
    unstageBtn_->setStyleSheet(
        "QPushButton { font-size: 10px; padding: 2px 6px; background: #5a3a1a; color: #ddd; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #7a5a3a; }");

    unstagedHeader_ = new QLabel("更改", changesWidget);
    unstagedHeader_->setStyleSheet("font-weight: bold; font-size: 10px;");
    unstagedTree_ = new QTreeWidget(changesWidget);
    unstagedTree_->setHeaderLabels({"文件", ""});
    unstagedTree_->setRootIsDecorated(false);
    unstagedTree_->header()->hide();
    unstagedTree_->header()->setStretchLastSection(true);
    unstagedTree_->setMaximumHeight(100);
    unstagedTree_->setAlternatingRowColors(true);
    unstagedTree_->setFont(QFont("Consolas", 8));

    stageBtn_ = new QPushButton("  +  暂存全部", changesWidget);
    stageBtn_->setMinimumHeight(28);
    stageBtn_->setStyleSheet(
        "QPushButton { font-size: 10px; padding: 2px 6px; background: #264f78; color: #fff; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #3a6ea5; }");

    commitMsgEdit_ = new QTextEdit(changesWidget);
    commitMsgEdit_->setPlaceholderText("提交消息 (留空自动生成)...");
    commitMsgEdit_->setMaximumHeight(56);
    commitMsgEdit_->setTabChangesFocus(true);
    commitMsgEdit_->setFont(QFont("Consolas", 9));

    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(3);

    auto makeBtn = [&](const QString& icon, const QString& tip) {
        auto* b = new QPushButton(icon, changesWidget);
        b->setToolTip(tip);
        b->setMinimumHeight(30);
        b->setMinimumWidth(36);
        b->setStyleSheet(
            "QPushButton { font-size: 14px; padding: 2px 6px; border: 1px solid #555; border-radius: 3px; background: #3c3c3c; color: #ccc; }"
            "QPushButton:hover { background: #505050; }"
            "QPushButton:disabled { color: #666; border-color: #444; }");
        return b;
    };

    commitBtn_ = makeBtn("\xe2\x9c\x93", "提交 (自动暂存所有更改)");  // checkmark
    pushBtn_   = makeBtn("\xe2\x86\x91", "推送到远程");                // up arrow
    pullBtn_   = makeBtn("\xe2\x86\x93", "从远程拉取");                // down arrow
    fetchBtn_  = makeBtn("\xe2\x86\x93\xe2\x86\x93", "获取远程");     // double down
    refreshBtn_= makeBtn("\xe2\x9f\xb3", "刷新");                      // refresh circle

    btnRow->addWidget(commitBtn_);
    btnRow->addWidget(pushBtn_);
    btnRow->addWidget(pullBtn_);
    btnRow->addWidget(fetchBtn_);
    btnRow->addWidget(refreshBtn_);

    changesLayout->addWidget(stagedHeader_);
    changesLayout->addWidget(stagedTree_);
    changesLayout->addWidget(unstageBtn_);
    changesLayout->addWidget(unstagedHeader_);
    changesLayout->addWidget(unstagedTree_);
    changesLayout->addWidget(stageBtn_);
    changesLayout->addWidget(commitMsgEdit_);
    changesLayout->addLayout(btnRow);

    vSplit->addWidget(historyTree_);
    vSplit->addWidget(changesWidget);
    vSplit->setStretchFactor(0, 3);
    vSplit->setStretchFactor(1, 2);

    layout->addWidget(vSplit, 1);

    connect(commitBtn_, &QPushButton::clicked, this, &GitPanel::onCommit);
    connect(pushBtn_, &QPushButton::clicked, this, &GitPanel::onPush);
    connect(pullBtn_, &QPushButton::clicked, this, &GitPanel::onPull);
    connect(fetchBtn_, &QPushButton::clicked, this, &GitPanel::onFetch);
    connect(refreshBtn_, &QPushButton::clicked, this, &GitPanel::onRefresh);
    connect(stageBtn_, &QPushButton::clicked, this, &GitPanel::onStageSelected);
    connect(unstageBtn_, &QPushButton::clicked, this, &GitPanel::onUnstageSelected);
    connect(unstagedTree_, &QTreeWidget::itemDoubleClicked, this, &GitPanel::onFileDoubleClicked);
    connect(stagedTree_, &QTreeWidget::itemDoubleClicked, this, &GitPanel::onFileDoubleClicked);
}

void GitPanel::setRepoPath(const std::string& path)
{
    repoPath_ = path;
    refresh();
}

void GitPanel::refresh()
{
    if (!refreshTimer_) return;
    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText("正在扫描仓库...");
    loadGitStatus();
    loadGitHistory();
    updateCounts();
}

void GitPanel::onRefresh()
{
    refresh();
}

void GitPanel::executeGit(const QStringList& args, QString& out, QString& err, int timeoutMs)
{
    if (repoPath_.empty()) return;

    QProcess proc;
    proc.setWorkingDirectory(QString::fromStdString(repoPath_));
    proc.start(gitPath_, args);
    proc.waitForFinished(timeoutMs);
    out = QString::fromUtf8(proc.readAllStandardOutput());
    err = QString::fromUtf8(proc.readAllStandardError());
}

void GitPanel::loadGitStatus()
{
    stagedFiles_.clear();
    unstagedFiles_.clear();
    stagedTree_->clear();
    unstagedTree_->clear();

    if (repoPath_.empty()) {
        branchLabel_->setText("未打开仓库");
        return;
    }

    QString out, err;
    executeGit({"status", "--porcelain", "-b"}, out, err);

    auto lines = out.split('\n', Qt::SkipEmptyParts);
    for (auto& line : lines) {
        if (line.startsWith("## ")) {
            auto parts = line.mid(3).split("...");
            currentBranch_ = parts.value(0);
            trackingRemote_ = parts.size() > 1 ? parts[1] : "";
            continue;
        }
        if (line.length() < 3) continue;

        GitFileEntry entry;
        entry.xStatus = line[0].toLatin1();
        entry.yStatus = line[1].toLatin1();
        entry.path = line.mid(3).trimmed();

        if (entry.xStatus != ' ' && entry.xStatus != '?') {
            entry.staged = true;
            stagedFiles_.push_back(entry);
        }
        if (entry.yStatus != ' ' || entry.xStatus == '?') {
            entry.staged = false;
            unstagedFiles_.push_back(entry);
        }
    }

    for (auto& f : stagedFiles_) {
        auto* item = new QTreeWidgetItem(stagedTree_);
        item->setText(0, f.path);
        item->setText(1, QString("%1%2").arg(f.xStatus).arg(f.yStatus));
    }
    for (auto& f : unstagedFiles_) {
        auto* item = new QTreeWidgetItem(unstagedTree_);
        item->setText(0, f.path);
        item->setText(1, QString("%1%2").arg(f.xStatus).arg(f.yStatus));
    }

    updateCounts();
}

void GitPanel::loadGitHistory()
{
    historyTree_->clear();
    if (repoPath_.empty()) return;

    QString out, err;
    executeGit({"log", "--oneline", "--graph", "--all", "-30", "--decorate"}, out, err);

    auto lines = out.split('\n', Qt::SkipEmptyParts);
    for (auto& line : lines) {
        auto* item = new QTreeWidgetItem(historyTree_);
        item->setText(0, line.trimmed());
    }
}

void GitPanel::updateCounts()
{
    int staged = static_cast<int>(stagedFiles_.size());
    int unstaged = static_cast<int>(unstagedFiles_.size());
    int total = staged + unstaged;

    if (repoPath_.empty()) {
        branchLabel_->setText("未打开仓库");
        statusLabel_->setText("");
    } else {
        QString branchText = QString("分支: %1").arg(currentBranch_);
        if (!trackingRemote_.isEmpty())
            branchText += QString("  →  %1").arg(trackingRemote_);
        branchLabel_->setText(branchText);

        QString statusText;
        if (total == 0)
            statusText = "工作区干净";
        else {
            statusText = QString("%1 个暂存, %2 个更改")
                .arg(staged).arg(unstaged);
        }
        statusLabel_->setText(statusText);
        statusLabel_->setStyleSheet(total > 0 ? "color: #c5862b;" : "color: green;");
    }

    stagedHeader_->setText(QString("暂存的更改 (%1)").arg(staged));
    unstagedHeader_->setText(QString("更改 (%1)").arg(unstaged));

    commitBtn_->setEnabled(total > 0);

    emit statusChanged(currentBranch_, total);
}

void GitPanel::onStageSelected()
{
    auto items = unstagedTree_->selectedItems();
    if (items.isEmpty()) return;

    for (auto* item : items) {
        QString path = item->text(0);
        QString out, err;
        executeGit({"add", "--", path}, out, err);
    }
    refresh();
}

void GitPanel::onUnstageSelected()
{
    auto items = stagedTree_->selectedItems();
    if (items.isEmpty()) return;

    for (auto* item : items) {
        QString path = item->text(0);
        QString out, err;
        executeGit({"reset", "HEAD", "--", path}, out, err);
    }
    refresh();
}

void GitPanel::onFileDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;
    QString path = item->text(0);

    auto* tree = item->treeWidget();
    if (tree == unstagedTree_) {
        QString out, err;
        executeGit({"add", "--", path}, out, err);
    } else if (tree == stagedTree_) {
        QString out, err;
        executeGit({"reset", "HEAD", "--", path}, out, err);
    }
    refresh();
}

void GitPanel::onCommit()
{
    int total = static_cast<int>(stagedFiles_.size() + unstagedFiles_.size());
    if (total == 0) return;

    if (stagedFiles_.empty() && !unstagedFiles_.empty()) {
        for (auto& f : unstagedFiles_) {
            QString out, err;
            executeGit({"add", "--", f.path}, out, err);
        }
        refresh();
        return;
    }

    QString msg = commitMsgEdit_->toPlainText().trimmed();

    if (msg.isEmpty()) {
        QStringList paths;
        for (auto& f : stagedFiles_)
            paths << f.path;
        QStringList shortPaths;
        for (auto& p : paths) {
            int slash = p.lastIndexOf('/');
            shortPaths << (slash >= 0 ? p.mid(slash + 1) : p);
        }
        msg = QString("更新 %1: %2")
            .arg(paths.size())
            .arg(shortPaths.join(", "));
    }

    QMessageBox dlg(this);
    dlg.setWindowTitle("确认提交");
    dlg.setText(QString("将提交 %1 个文件").arg(stagedFiles_.size()));
    dlg.setInformativeText(msg);
    dlg.setDetailedText(
        QString("文件列表:\n") + [&]() {
            QStringList l;
            for (auto& f : stagedFiles_) l << ("  " + f.path);
            return l.join("\n");
        }());
    dlg.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    dlg.setDefaultButton(QMessageBox::Yes);
    dlg.button(QMessageBox::Yes)->setText("提交");
    dlg.button(QMessageBox::No)->setText("取消");

    if (dlg.exec() != QMessageBox::Yes) return;

    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText("正在提交...");
    QApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString out, err;
    executeGit({"commit", "-m", msg}, out, err, 60000);
    QApplication::restoreOverrideCursor();

    if (!err.isEmpty() && !err.contains("nothing to commit")) {
        QMessageBox::critical(this, "提交失败", err);
    } else {
        qInfo() << "Git: commit success - " << msg;
        commitMsgEdit_->clear();
        emit commitFinished(currentBranch_);
    }

    refresh();
}

void GitPanel::onPush()
{
    if (repoPath_.empty()) return;

    auto reply = QMessageBox::question(this, "确认推送",
        QString("推送 %1 到远程仓库?").arg(currentBranch_),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText("正在推送...");
    QApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString out, err;
    executeGit({"push", "origin", currentBranch_}, out, err, 120000);
    QApplication::restoreOverrideCursor();

    if (!err.isEmpty() && !err.contains("Everything up-to-date")) {
        QMessageBox::critical(this, "推送失败",
            QString("%1\n\n如果远程需要认证，请确认已配置 SSH 密钥或凭据。").arg(err));
    } else {
        qInfo() << "Git: push success - " << currentBranch_;
        QMessageBox::information(this, "推送完成", "推送成功。");
    }
    refresh();
}

void GitPanel::onPull()
{
    if (repoPath_.empty()) return;

    auto reply = QMessageBox::question(this, "确认拉取",
        "从远程仓库拉取最新更改?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText("正在拉取...");
    QApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString out, err;
    executeGit({"pull", "origin", currentBranch_}, out, err, 120000);
    QApplication::restoreOverrideCursor();

    if (!err.isEmpty() && !err.contains("Already up to date")) {
        QMessageBox::critical(this, "拉取失败", err);
    } else {
        qInfo() << "Git: pull success";
    }
    refresh();
}

void GitPanel::onFetch()
{
    if (repoPath_.empty()) return;

    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText("正在获取...");
    QApplication::processEvents();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString out, err;
    executeGit({"fetch", "--all"}, out, err, 60000);
    QApplication::restoreOverrideCursor();

    if (!err.isEmpty())
        QMessageBox::critical(this, "获取失败", err);

    qInfo() << "Git: fetch complete";
    refresh();
}

} // namespace HiBerGUI
