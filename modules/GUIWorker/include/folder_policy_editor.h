#pragma once

#include <QWidget>
#include <QLabel>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>

namespace GUIWorker {

// 文件夹同步策略编辑器（P2）：四选一 + 跟随默认，写入顶层或分支级 sync_policies
class FolderPolicyEditor : public QWidget {
    Q_OBJECT

public:
    explicit FolderPolicyEditor(QWidget* parent = nullptr);

    // effectivePolicy: 当前生效策略（合并后）；branchOverrides: 分支级是否有覆盖
    void load(const QString& folderPath, const QString& effectivePolicy,
        bool branchOverrides, const QString& branchName);

public slots:
    void setScopeTop();

signals:
    void saveRequested(const QString& folderPath, const QString& policy,
        bool toBranch);
    void contentModified();

private:
    QLabel* pathLabel_;
    QLabel* stateLabel_;
    QRadioButton* inheritRb_;
    QRadioButton* skipRb_;
    QRadioButton* mirrorRb_;
    QRadioButton* addRb_;
    QRadioButton* overwriteRb_;
    QRadioButton* topRb_;
    QRadioButton* branchRb_;
    QPushButton* saveButton_;

    QString folderPath_;
    QString branchName_;

    void applyStyle();
};

} // namespace GUIWorker
