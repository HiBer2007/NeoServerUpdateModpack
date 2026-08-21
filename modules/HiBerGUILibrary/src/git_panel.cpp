#include "git_panel.h"
#include "animated_progress.h"

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
#include <QAction>
#include <QRegularExpression>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QListWidget>
#include <algorithm>
#include <crtdbg.h>

namespace HiBerGUI {

GitPanel::GitPanel(QWidget* parent)
    : QWidget(parent), refreshTimer_(nullptr)
{
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

    // 常驻顶部忙碌条: 3-5px 细条, 空闲隐藏动画, 忙碌时脉冲
    busyBar_ = new AnimatedProgress(this);
    busyBar_->setCompact(true);
    layout->addWidget(busyBar_);

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
    historyTree_->setUniformRowHeights(true);
    historyTree_->setFont(QFont("Consolas", 8));
    historyTree_->setMinimumHeight(60);

    auto* changesWidget = new QWidget(vSplit);
    auto* changesLayout = new QVBoxLayout(changesWidget);
    changesLayout->setContentsMargins(0, 4, 0, 0);
    changesLayout->setSpacing(2);

    stagedHeader_ = new QLabel("暂存", changesWidget);
    stagedHeader_->setStyleSheet("font-weight: bold; font-size: 10px;");
    // 垂直 sizePolicy 固定, 防止 splitter 拉伸布局时 QLabel 被拉高
    stagedHeader_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    stagedTree_ = new QTreeWidget(changesWidget);
    stagedTree_->setHeaderLabels({"文件", "状态"});
    stagedTree_->setRootIsDecorated(false);
    stagedTree_->header()->hide();
    // 路径列占满剩余空间, 状态列固定宽度靠右显示 (A/M/D/??)
    stagedTree_->header()->setStretchLastSection(false);
    stagedTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    stagedTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    stagedTree_->setMaximumHeight(80);
    stagedTree_->setAlternatingRowColors(true);
    stagedTree_->setUniformRowHeights(true);
    stagedTree_->setFont(QFont("Consolas", 8));

    unstageBtn_ = new QPushButton("  -  取消暂存", changesWidget);
    unstageBtn_->setMinimumHeight(28);
    unstageBtn_->setStyleSheet(
        "QPushButton { font-size: 10px; padding: 2px 6px; background: #5a3a1a; color: #ddd; border: none; border-radius: 3px; }"
        "QPushButton:hover { background: #7a5a3a; }");

    unstagedHeader_ = new QLabel("更改", changesWidget);
    unstagedHeader_->setStyleSheet("font-weight: bold; font-size: 10px;");
    unstagedHeader_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    unstagedTree_ = new QTreeWidget(changesWidget);
    unstagedTree_->setHeaderLabels({"文件", "状态"});
    unstagedTree_->setRootIsDecorated(false);
    unstagedTree_->header()->hide();
    unstagedTree_->header()->setStretchLastSection(false);
    unstagedTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    unstagedTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    unstagedTree_->setMaximumHeight(100);
    unstagedTree_->setAlternatingRowColors(true);
    unstagedTree_->setUniformRowHeights(true);
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

    // 历史记录右键菜单: 撤回/回退指定提交
    historyTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(historyTree_, &QTreeWidget::customContextMenuRequested,
        this, &GitPanel::onHistoryContextMenu);

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
}

void GitPanel::onRefresh()
{
    refresh();
}

void GitPanel::setGitBusy(bool busy)
{
    gitBusy_ = busy;
    commitBtn_->setEnabled(!busy);
    pushBtn_->setEnabled(!busy);
    pullBtn_->setEnabled(!busy);
    fetchBtn_->setEnabled(!busy);
    stageBtn_->setEnabled(!busy);
    unstageBtn_->setEnabled(!busy);
    if (!busy) {
        commitBtn_->setEnabled(!stagedFiles_.empty() || !unstagedFiles_.empty());
    }
}

void GitPanel::beginBusy(const QString& text)
{
    statusLabel_->setStyleSheet("color: #007acc; font-size: 10px;");
    statusLabel_->setText(text);
    busyBar_->setText(text);
    busyBar_->setIndeterminate(true);
    setGitBusy(true);
    QApplication::processEvents();
}

void GitPanel::endBusy()
{
    busyBar_->setIndeterminate(false);
    setGitBusy(false);
    refresh();
}

int GitPanel::executeGit(const QStringList& args, QString& out, QString& err,
    int timeoutMs)
{
    if (repoPath_.empty()) return -1;

    QProcess proc;
    proc.setWorkingDirectory(QString::fromStdString(repoPath_));
    proc.start(gitPath_, args);
    proc.waitForFinished(timeoutMs);
    out = QString::fromUtf8(proc.readAllStandardOutput());
    err = QString::fromUtf8(proc.readAllStandardError());
    return proc.exitCode();
}

void GitPanel::runGitAsync(const QStringList& args, int timeoutMs,
    std::function<void(bool, const QString&)> onDone)
{
    if (repoPath_.empty()) {
        if (onDone) onDone(false, QStringLiteral("repo path empty"));
        return;
    }

    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(QString::fromStdString(repoPath_));
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->setProperty("_nsumTimeoutMs", timeoutMs);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QByteArray data = proc->readAllStandardOutput();
        const QString text = QString::fromUtf8(data);
        for (auto& line : text.split('\n', Qt::SkipEmptyParts)) {
            emit gitOutput(line.trimmed());
        }
    });

    auto finish = [this, proc, onDone](int exitCode, QProcess::ExitStatus status) {
        const QString tail = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        for (auto& line : tail.split('\n', Qt::SkipEmptyParts)) {
            emit gitOutput(line.trimmed());
        }
        QString errMsg = tail;
        if (status == QProcess::CrashExit) {
            // git 正常执行不会 CrashExit: 由超时 kill 触发 (或进程崩溃)
            const int tmo = proc->property("_nsumTimeoutMs").toInt();
            errMsg = tmo > 0
                ? QStringLiteral("git process killed after %1 ms (timeout or crash)").arg(tmo)
                : QStringLiteral("git process terminated abnormally (crashed)");
        } else if (exitCode != 0 && errMsg.isEmpty()) {
            errMsg = QStringLiteral("git exited with code %1").arg(exitCode);
        }
        const bool ok = (status == QProcess::NormalExit && exitCode == 0);
        if (ok) {
            emit gitOperationFinished(true, QString());
        } else {
            emit gitOperationFinished(false, errMsg);
        }
        if (onDone) onDone(ok, errMsg);
        proc->deleteLater();
    };

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [finish](int exitCode, QProcess::ExitStatus status) {
            finish(exitCode, status);
        });
    // 启动失败 (如命令行过长) 不触发 finished, 必须单独处理, 否则操作静默无输出
    connect(proc, &QProcess::errorOccurred, this,
        [this, proc, onDone](QProcess::ProcessError err) {
            if (err != QProcess::FailedToStart) return;
            const QString msg = QStringLiteral("failed to start git: %1")
                .arg(proc->errorString());
            emit gitOperationFinished(false, msg);
            if (onDone) onDone(false, msg);
            proc->deleteLater();
        });

    // 超时: timeoutMs <= 0 表示无限等待 (不启动 kill 定时器)
    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, proc, [proc]() {
            if (proc->state() != QProcess::NotRunning) {
                proc->kill();
            }
        });
    }

    proc->start(gitPath_, args);
}

void GitPanel::loadGitStatus()
{
    if (repoPath_.empty()) {
        branchLabel_->setText("未打开仓库");
        updateCounts();
        return;
    }

    const quint64 gen = ++statusGen_;
    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(QString::fromStdString(repoPath_));
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, gen](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAllStandardOutput());
            proc->deleteLater();
            if (gen != statusGen_) return;

            stagedFiles_.clear();
            unstagedFiles_.clear();
            // 抑制更新, 避免 clear + 重建过程整树重绘闪烁
            stagedTree_->setUpdatesEnabled(false);
            unstagedTree_->setUpdatesEnabled(false);
            stagedTree_->clear();
            unstagedTree_->clear();

            // -z 输出: 每条记录以 NUL 结尾, 路径不带引号转义 (含空格/中文路径安全)
            const QStringList records = out.split(QChar('\0'), Qt::SkipEmptyParts);
            for (auto& rec : records) {
                if (rec.isEmpty()) continue;
                if (rec.startsWith("## ")) {
                    auto parts = rec.mid(3).split("...");
                    currentBranch_ = parts.value(0);
                    trackingRemote_ = parts.size() > 1 ? parts[1] : "";
                    continue;
                }
                if (rec.length() < 3) continue;

                GitFileEntry entry;
                entry.xStatus = rec[0].toLatin1();
                entry.yStatus = rec[1].toLatin1();
                entry.path = rec.mid(3).trimmed();

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
                item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
                item->setToolTip(0, f.path);
            }
            for (auto& f : unstagedFiles_) {
                auto* item = new QTreeWidgetItem(unstagedTree_);
                item->setText(0, f.path);
                item->setText(1, QString("%1%2").arg(f.xStatus).arg(f.yStatus));
                item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
                item->setToolTip(0, f.path);
            }

            stagedTree_->setUpdatesEnabled(true);
            unstagedTree_->setUpdatesEnabled(true);
            stagedTree_->viewport()->update();
            unstagedTree_->viewport()->update();
            updateCounts();
        });

    proc->start(gitPath_, { QStringLiteral("status"), QStringLiteral("--porcelain"),
        QStringLiteral("-b"), QStringLiteral("-z") });
}

void GitPanel::loadGitHistory()
{
    // 不清空历史列表: 异步读取期间保留旧内容, 避免空白闪烁
    if (repoPath_.empty()) return;

    const quint64 gen = ++historyGen_;
    auto* proc = new QProcess(this);
    proc->setWorkingDirectory(QString::fromStdString(repoPath_));
    proc->setProcessChannelMode(QProcess::SeparateChannels);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [this, proc, gen](int, QProcess::ExitStatus) {
            const QString out = QString::fromUtf8(proc->readAllStandardOutput());
            proc->deleteLater();
            if (gen != historyGen_) return;

            // 抑制更新, 避免 clear + 重建过程整树重绘闪烁
            historyTree_->setUpdatesEnabled(false);
            historyTree_->clear();
            // 从 "| * 1234567 commit msg" 行中提取提交哈希 (7位+)
            QRegularExpression hashRe("[0-9a-f]{7,40}");
            auto lines = out.split('\n', Qt::SkipEmptyParts);
            for (auto& line : lines) {
                auto* item = new QTreeWidgetItem(historyTree_);
                item->setText(0, line.trimmed());
                item->setToolTip(0, line.trimmed());
                auto m = hashRe.match(line);
                if (m.hasMatch())
                    item->setData(0, Qt::UserRole, m.captured(0));
            }
            historyTree_->setUpdatesEnabled(true);
            historyTree_->viewport()->update();
        });

    proc->start(gitPath_, { QStringLiteral("log"), QStringLiteral("--oneline"),
        QStringLiteral("--graph"), QStringLiteral("--all"), QStringLiteral("-30"),
        QStringLiteral("--decorate") });
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
        statusLabel_->setStyleSheet(total > 0
            ? "color: #c5862b; font-size: 10px;"
            : "color: green; font-size: 10px;");
    }

    stagedHeader_->setText(QString("暂存的更改 (%1)").arg(staged));
    unstagedHeader_->setText(QString("更改 (%1)").arg(unstaged));

    commitBtn_->setEnabled(total > 0);

    emit statusChanged(currentBranch_, total);
}

void GitPanel::onStageSelected()
{
    if (repoPath_.empty() || gitBusy_) return;

    auto items = unstagedTree_->selectedItems();
    QStringList paths;
    if (items.isEmpty()) {
        // 未选中任何项: 暂存所有未暂存文件
        if (unstagedFiles_.empty()) return;
        for (auto& f : unstagedFiles_) paths << f.path;
    } else {
        for (auto* item : items) paths << item->text(0);
    }
    if (paths.isEmpty()) return;

    beginBusy("正在暂存...");
    runGitAsync(QStringList{"add", "--"} + paths, -1,
        [this, paths](bool ok, const QString& errMsg) {
            endBusy();
            if (ok) {
                emit gitOutput(
                    QStringLiteral("staged %1 file(s)").arg(paths.size()));
            } else {
                QMessageBox::critical(this, "暂存失败", errMsg);
            }
        });
}

void GitPanel::onUnstageSelected()
{
    if (repoPath_.empty() || gitBusy_) return;

    auto items = stagedTree_->selectedItems();
    QStringList paths;
    if (items.isEmpty()) {
        // 未选中任何项: 取消暂存所有暂存文件
        for (auto& f : stagedFiles_) paths << f.path;
    } else {
        for (auto* item : items) paths << item->text(0);
    }
    if (paths.isEmpty()) return;

    QString out, err;
    // 无 HEAD 的初始仓库 (首次暂存) 不能 git restore --staged / reset HEAD:
    // HEAD 解析失败 (exit 128)。对新增文件改用 git rm --cached (同步探测, 本地毫秒级)
    const bool hasHead = (executeGit({"rev-parse", "--verify", "HEAD"}, out, err) == 0);

    beginBusy("正在取消暂存...");
    const QStringList args = hasHead
        ? (QStringList{"restore", "--staged", "--"} + paths)
        : (QStringList{"rm", "--cached", "--"} + paths);
    runGitAsync(args, -1,
        [this, paths](bool ok, const QString& errMsg) {
            endBusy();
            if (ok) {
                emit gitOutput(
                    QStringLiteral("unstaged %1 file(s)").arg(paths.size()));
            } else {
                QMessageBox::critical(this, "取消暂存失败", errMsg);
            }
        });
}

void GitPanel::onFileDoubleClicked(QTreeWidgetItem* item, int)
{
    if (!item || gitBusy_) return;
    QString path = item->text(0);

    auto* tree = item->treeWidget();
    if (tree == unstagedTree_) {
        beginBusy("正在暂存...");
        runGitAsync(QStringList{"add", "--", path}, -1,
            [this](bool ok, const QString& errMsg) {
                endBusy();
                if (ok) {
                    emit gitOutput(QStringLiteral("staged 1 file(s)"));
                } else {
                    QMessageBox::critical(this, "暂存失败", errMsg);
                }
            });
    } else if (tree == stagedTree_) {
        // 与 onUnstageSelected 同款: 无 HEAD 初始仓库用 git rm --cached
        QString out, err;
        const bool hasHead = (executeGit({"rev-parse", "--verify", "HEAD"}, out, err) == 0);
        beginBusy("正在取消暂存...");
        const QStringList args = hasHead
            ? QStringList{"restore", "--staged", "--", path}
            : QStringList{"rm", "--cached", "--", path};
        runGitAsync(args, -1,
            [this](bool ok, const QString& errMsg) {
                endBusy();
                if (ok) {
                    emit gitOutput(QStringLiteral("unstaged 1 file(s)"));
                } else {
                    QMessageBox::critical(this, "取消暂存失败", errMsg);
                }
            });
    }
}

void GitPanel::onCommit()
{
    int total = static_cast<int>(stagedFiles_.size() + unstagedFiles_.size());
    if (total == 0) return;

    if (stagedFiles_.empty() && !unstagedFiles_.empty()) {
        QStringList paths;
        for (auto& f : unstagedFiles_) paths << f.path;
        beginBusy("正在暂存...");
        runGitAsync(QStringList{"add", "--"} + paths, -1,
            [this](bool ok, const QString& errMsg) {
                endBusy();
                if (!ok) {
                    QMessageBox::critical(this, "暂存失败", errMsg);
                }
            });
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

    beginBusy("正在提交...");

    // 异步执行, 不阻塞 GUI; 输出实时转发终端, 失败弹窗报错
    runGitAsync(QStringList{"commit", "-m", msg}, -1,
        [this, msg](bool ok, const QString& errMsg) {
            endBusy();
            if (!ok && !errMsg.contains("nothing to commit")) {
                QMessageBox::critical(this, "提交失败", errMsg);
            } else {
                qInfo() << "Git: commit success - " << msg;
                commitMsgEdit_->clear();
                emit commitFinished(currentBranch_);
                emit historyChanged(currentBranch_);
            }
        });
}

void GitPanel::onPush()
{
    if (repoPath_.empty()) return;

    auto reply = QMessageBox::question(this, "确认推送",
        QString("推送 %1 到远程仓库?").arg(currentBranch_),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    beginBusy("正在推送...");

    runGitAsync(QStringList{"push", "origin", currentBranch_}, -1,
        [this](bool ok, const QString& errMsg) {
            endBusy();
            if (!ok && !errMsg.contains("Everything up-to-date")) {
                QMessageBox::critical(this, "推送失败",
                    QString("%1\n\n如果远程需要认证，请确认已配置 SSH 密钥或凭据。")
                        .arg(errMsg));
            } else {
                qInfo() << "Git: push success - " << currentBranch_;
                QMessageBox::information(this, "推送完成", "推送成功。");
            }
        });
}

void GitPanel::onPull()
{
    if (repoPath_.empty()) return;

    auto reply = QMessageBox::question(this, "确认拉取",
        "从远程仓库拉取最新更改?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    beginBusy("正在拉取...");

    runGitAsync(QStringList{"pull", "origin", currentBranch_}, -1,
        [this](bool ok, const QString& errMsg) {
            endBusy();
            if (!ok && !errMsg.contains("Already up to date")) {
                QMessageBox::critical(this, "拉取失败", errMsg);
            } else {
                qInfo() << "Git: pull success";
            }
        });
}

void GitPanel::onFetch()
{
    if (repoPath_.empty() || gitBusy_) return;

    beginBusy("正在获取...");

    runGitAsync(QStringList{"fetch", "--all"}, -1,
        [this](bool ok, const QString& errMsg) {
            endBusy();
            if (!ok) {
                QMessageBox::critical(this, "获取失败", errMsg);
            }
            qInfo() << "Git: fetch complete";
        });
}

void GitPanel::softResetTo(const QString& hash)
{
    if (repoPath_.empty() || gitBusy_ || hash.trimmed().isEmpty()) return;

    beginBusy("正在软回退...");
    runGitAsync(QStringList{"reset", "--soft", hash.trimmed()}, -1,
        [this, hash](bool ok, const QString& errMsg) {
            endBusy();
            if (!ok) {
                QMessageBox::critical(this, "软回退失败", errMsg);
            } else {
                qInfo() << "Git: soft reset to" << hash;
                emit gitOutput(QStringLiteral(
                    "soft reset to %1 (working tree kept, stage retained; "
                    "commit again to squash)").arg(hash));
                emit historyChanged(currentBranch_);
                refresh();
            }
        });
}

void GitPanel::squashDialog()
{
    if (repoPath_.empty() || gitBusy_) return;

    // 同步读取当前分支线性历史 (本地毫秒级), 供对话框选择目标提交
    QString out, err;
    if (executeGit({"log", "--oneline", "HEAD", "-50"}, out, err) != 0) {
        QMessageBox::warning(this, "合并提交",
            QString("无法读取提交历史:\n%1").arg(err.trimmed()));
        return;
    }
    const QStringList lines = out.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        QMessageBox::information(this, "合并提交",
            "历史不足两条，没有可合并的提交。");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle("合并提交 (squash)");
    dlg.setMinimumSize(560, 460);
    auto* lay = new QVBoxLayout(&dlg);

    auto* tip = new QLabel(
        "选择要合并到的目标提交：将把所选提交及其之后的所有提交（闭区间）"
        "合并为一个新提交，工作区/暂存区不变。", &dlg);
    tip->setWordWrap(true);
    lay->addWidget(tip);

    auto* list = new QListWidget(&dlg);
    QStringList hashes;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines[i].trimmed();
        auto* item = new QListWidgetItem(line, list);
        hashes << line.left(line.indexOf(QLatin1Char(' ')));
        if (i == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setText(line + QStringLiteral("  (当前 HEAD)"));
            item->setToolTip(QStringLiteral("\u4e0d\u80fd\u5408\u5e76\u5230 HEAD \u81ea\u8eab"));
        }
    }
    list->setCurrentRow(1);
    lay->addWidget(list, 1);

    lay->addWidget(new QLabel("合并后的提交消息:", &dlg));
    auto* msgEdit = new QPlainTextEdit(&dlg);
    const QString headLine = lines.value(0);
    const int sp = headLine.indexOf(QLatin1Char(' '));
    msgEdit->setPlainText(sp >= 0 ? headLine.mid(sp + 1).trimmed() : QString());
    msgEdit->setMaximumHeight(90);
    lay->addWidget(msgEdit);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText("合并提交");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    lay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    const int row = list->currentRow();
    if (row <= 0 || row >= hashes.size()) return;
    const QString msg = msgEdit->toPlainText().trimmed();
    if (msg.isEmpty()) {
        QMessageBox::warning(this, "合并提交", "提交消息不能为空。");
        return;
    }
    // 闭区间合并: 所选提交本身也要并入, 软回退到其父提交
    const QString target = hashes[row];
    QString parentOut, parentErr;
    if (executeGit({"rev-parse", target + QStringLiteral("^")},
            parentOut, parentErr) != 0) {
        QMessageBox::warning(this, "合并提交",
            "所选提交没有父提交（已是最早提交），无法合并。");
        return;
    }
    squashTo(parentOut.trimmed(), msg);
}

void GitPanel::squashTo(const QString& hash, const QString& message)
{
    if (repoPath_.empty() || gitBusy_ || hash.trimmed().isEmpty()) return;

    beginBusy("正在合并提交...");
    runGitAsync(QStringList{"reset", "--soft", hash.trimmed()}, -1,
        [this, hash, message](bool ok, const QString& errMsg) {
            if (!ok) {
                endBusy();
                QMessageBox::critical(this, "合并提交失败", errMsg);
                return;
            }
            runGitAsync(QStringList{"commit", "-m", message}, -1,
                [this, hash](bool ok2, const QString& errMsg2) {
                    endBusy();
                    if (!ok2) {
                        QMessageBox::critical(this, "合并提交失败", errMsg2);
                        return;
                    }
                    emit gitOutput(QStringLiteral(
                        "squashed commits into one on top of %1").arg(hash));
                    emit historyChanged(currentBranch_);
                    refresh();
                });
        });
}

void GitPanel::onHistoryContextMenu(const QPoint& pos)
{
    auto* item = historyTree_->itemAt(pos);
    if (!item) return;
    // menu.exec 是模态事件循环: 期间异步刷新 (loadGitHistory) 可能 clear+重建树,
    // 使 item 悬垂 -> 必须在 exec 前提取所需数据, exec 后不得再访问 item
    const QString hash = item->data(0, Qt::UserRole).toString();
    if (hash.isEmpty()) return;

    QMenu menu(this);
    QAction* revertAct = menu.addAction("撤回此提交");
    QAction* resetAct = menu.addAction("回退到此提交");
    QAction* softResetAct = menu.addAction("软回退到此提交（保留工作区）");
    QAction* chosen = menu.exec(historyTree_->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == softResetAct) {
        auto reply = QMessageBox::warning(this, "确认软回退",
            QString("将回滚历史到 %1，但保留工作区与暂存区（不修改任何文件内容）。\n\n"
                    "随后重新提交即可将之后的更改合并为一个提交。继续?").arg(hash),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        softResetTo(hash);
    } else if (chosen == revertAct) {
        auto reply = QMessageBox::question(this, "确认撤回",
            QString("将生成反向提交以撤销 %1 的更改，保留历史。继续?").arg(hash),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        beginBusy("正在撤回...");
        runGitAsync(QStringList{"revert", "--no-edit", hash}, -1,
            [this](bool ok, const QString& errMsg) {
                endBusy();
                if (!ok) {
                    QMessageBox::critical(this, "撤回失败", errMsg);
                    // 仅当 revert 实际进入进行中状态 (REVERT_HEAD 存在) 才中止;
                    // 本地未提交更改导致的校验失败不会进入 revert 状态, abort 会误报
                    QString out, err;
                    const bool inProgress = (executeGit(
                        {"rev-parse", "--verify", "REVERT_HEAD"}, out, err) == 0);
                    if (inProgress) {
                        runGitAsync(QStringList{"revert", "--abort"}, -1, nullptr);
                    }
                } else {
                    qInfo() << "Git: revert success";
                    emit historyChanged(currentBranch_);
                }
            });
    } else if (chosen == resetAct) {
        auto reply = QMessageBox::warning(this, "确认回退",
            QString("将丢弃 %1 之后的所有提交与未提交更改，不可恢复。继续?").arg(hash),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
        beginBusy("正在回退...");
        runGitAsync(QStringList{"reset", "--hard", hash}, -1,
            [this](bool ok, const QString& errMsg) {
                endBusy();
                if (!ok) {
                    QMessageBox::critical(this, "回退失败", errMsg);
                } else {
                    qInfo() << "Git: reset --hard success";
                    emit historyChanged(currentBranch_);
                }
            });
    } else {
        refresh();
    }
}

} // namespace HiBerGUI
