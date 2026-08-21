#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <QMenu>
#include <string>
#include <vector>
#include <functional>

namespace HiBerGUI {

class AnimatedProgress;

struct GitFileEntry {
    QString path;
    char xStatus = ' ';
    char yStatus = ' ';
    bool staged = false;
};

class GitPanel : public QWidget {
    Q_OBJECT

public:
    explicit GitPanel(QWidget* parent = nullptr);
    ~GitPanel();

    void setRepoPath(const std::string& path);
    std::string repoPath() const { return repoPath_; }
    void refresh();

    // 设置 git 可执行文件路径 (默认 "git" 走 PATH; 宿主可注入内置/自定 git)
    void setGitPath(const QString& path) { gitPath_ = path; }
    QString gitPath() const { return gitPath_; }

    // 软回退 (reset --soft): 回滚历史到目标提交, 工作区与暂存区不变 (合并提交用)
    void softResetTo(const QString& hash);

    // 合并提交对话框 (squash): GUI 选择目标提交 + 输入最终提交消息
    void squashDialog();

    bool hasChanges() const
    {
        return !repoPath_.empty() &&
            (!unstagedFiles_.empty() || !stagedFiles_.empty());
    }

signals:
    void statusChanged(const QString& branch, int totalChanges);
    void commitFinished(const QString& branch);
    // git 历史变更 (commit/revert/reset) 后触发, 宿主用于刷新工作区
    void historyChanged(const QString& branch);
    // git 进程输出实时转发 (宿主可挂到终端/日志)
    void gitOutput(const QString& line);
    // 异步 git 操作完成: ok=false 时 errMsg 含错误详情 (用于弹窗/日志)
    void gitOperationFinished(bool ok, const QString& errMsg);

private slots:
    void onCommit();
    void onPush();
    void onPull();
    void onFetch();
    void onStageSelected();
    void onUnstageSelected();
    void onRefresh();
    void onFileDoubleClicked(QTreeWidgetItem* item, int column);
    void onHistoryContextMenu(const QPoint& pos);

private:
    void buildUI();
    void loadGitStatus();
    void loadGitHistory();
    void updateCounts();

    // 同步执行 git: timeoutMs <= 0 表示无限等待
    int executeGit(const QStringList& args, QString& out, QString& err,
        int timeoutMs = -1);
    // 软回退 + 重新提交 (squash 执行链)
    void squashTo(const QString& hash, const QString& message);
    // 异步执行 git (不阻塞 GUI): 输出实时 emit gitOutput, 完成后回调 onDone(ok, errMsg)
    void runGitAsync(const QStringList& args, int timeoutMs,
        std::function<void(bool, const QString&)> onDone);
    // 异步 git 操作期间禁用操作按钮防重入
    void setGitBusy(bool busy);
    // 显式忙碌动画: 提交/推送/拉取/获取/暂存/取消暂存期间显示进度条 + 状态文本
    void beginBusy(const QString& text);
    void endBusy();

    std::string repoPath_;
    QString gitPath_ = QStringLiteral("git");

    QLabel* branchLabel_;
    QLabel* statusLabel_;

    AnimatedProgress* busyBar_;
    bool gitBusy_ = false;

    // 异步刷新代际号: 过期结果直接丢弃, 防止慢扫描覆盖新状态
    quint64 statusGen_ = 0;
    quint64 historyGen_ = 0;

    QLabel* stagedHeader_;
    QTreeWidget* stagedTree_;
    QPushButton* unstageBtn_;

    QLabel* unstagedHeader_;
    QTreeWidget* unstagedTree_;
    QPushButton* stageBtn_;

    QTextEdit* commitMsgEdit_;
    QPushButton* commitBtn_;
    QPushButton* pushBtn_;
    QPushButton* pullBtn_;
    QPushButton* fetchBtn_;
    QPushButton* refreshBtn_;

    QTreeWidget* historyTree_;

    QTimer* refreshTimer_;

    std::vector<GitFileEntry> stagedFiles_;
    std::vector<GitFileEntry> unstagedFiles_;

    QString currentBranch_;
    QString trackingRemote_;
};

} // namespace HiBerGUI
