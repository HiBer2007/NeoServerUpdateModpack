#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QSplitter>
#include <string>
#include <vector>

namespace HiBerGUI {

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

    bool hasChanges() const
    {
        return !repoPath_.empty() &&
            (!unstagedFiles_.empty() || !stagedFiles_.empty());
    }

signals:
    void statusChanged(const QString& branch, int totalChanges);
    void commitFinished(const QString& branch);

private slots:
    void onCommit();
    void onPush();
    void onPull();
    void onFetch();
    void onStageSelected();
    void onUnstageSelected();
    void onRefresh();
    void onFileDoubleClicked(QTreeWidgetItem* item, int column);

private:
    void buildUI();
    void loadGitStatus();
    void loadGitHistory();
    void updateCounts();

    void executeGit(const QStringList& args, QString& out, QString& err, int timeoutMs = 30000);

    std::string repoPath_;
    QString gitPath_ = QStringLiteral("git");

    QLabel* branchLabel_;
    QLabel* statusLabel_;

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
