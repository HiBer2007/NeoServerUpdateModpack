#pragma once

#include <animated_progress.h>

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QProcess>
#include <QList>
#include <string>
#include <vector>

namespace GUIWorker {

using HiBerGUI::AnimatedProgress;

struct GitBranchInfo {
    std::string name;
    std::string description;
    bool        isDefault = false;
    bool        hidden = false;
};

class BranchPage : public QWidget {
    Q_OBJECT

public:
    explicit BranchPage(QWidget* parent = nullptr);
    ~BranchPage() override;

    void     loadBranches(const QString& repoPath);
    void     showLoading(const QString& status, int percent = -1);
    void     stopLoading();
    QString  selectedBranch() const;
    bool     hasSelection() const;
    void     selectBranch(const QString& name);
    int      contentHeight(int widthHint = 500) const;

signals:
    void branchSelected(QString branch);
    void branchesLoaded(bool success);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QLabel*         repoLabel_   = nullptr;
    AnimatedProgress* progress_  = nullptr;
    QLabel*         statusLabel_ = nullptr;
    QFrame*         cardContainer_ = nullptr;
    QScrollArea*    scroll_      = nullptr;
    QList<QFrame*>  cards_;

    QString repoPath_;
    std::vector<GitBranchInfo> branches_;
    int     selectedIndex_ = -1;
    bool    darkMode_ = false;

    void populateUI();
    void updateSelection();
    void applyTheme();
    QString cardStyle(bool selected) const;
    void    collectBranches();
    void    readBranchInfo(GitBranchInfo& info) const;
};

} // namespace GUIWorker
