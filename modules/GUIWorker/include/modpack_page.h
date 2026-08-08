#pragma once

#include <animated_progress.h>

#include <QWidget>
#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QList>
#include <string>
#include <vector>

namespace GUIWorker {

using HiBerGUI::AnimatedProgress;

struct ModpackBranchInfo {
    std::string name;
    std::string parent;
    std::string gameVersion;
    std::string modloader;
    std::string modloaderVersion;
    std::string description;
    bool hidden = false;
};

class ModpackPage : public QWidget {
    Q_OBJECT

public:
    explicit ModpackPage(QWidget* parent = nullptr);
    ~ModpackPage() override;

    void     loadModpacks(const QString& repoPath);
    QString  selectedModpack() const;
    bool     hasSelection() const;
    void     selectModpack(const QString& name);
    QString  summary(const QString& branchName) const;
    int      contentHeight(int widthHint = 500) const;

signals:
    void modpackSelected(QString modpackBranch);
    void modpacksLoaded(bool success);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    QFrame* createCard(const ModpackBranchInfo& info);

    QFrame*        cardContainer_  = nullptr;
    QScrollArea*   scroll_         = nullptr;
    AnimatedProgress* progress_    = nullptr;
    QLabel*        statusLabel_    = nullptr;
    QList<QFrame*> cards_;

    QString                        repoPath_;
    std::vector<ModpackBranchInfo> branches_;
    int                            selectedIndex_ = -1;
    bool                           darkMode_ = false;

    void populateCards();
    void updateSelection();
    void applyTheme();
    QString cardStyle(bool selected) const;
    const ModpackBranchInfo* findBranch(const std::string& name) const;
};

} // namespace GUIWorker