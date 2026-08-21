#pragma once

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QRadioButton>
#include <QGroupBox>

#include <string>
#include <vector>

#include "repo_tree_panel.h"

namespace GUIWorker {

// 批量编辑器: 多选后进入的统一编辑页. 通用区(批量改同步策略/删除)恒显示,
// 同种选集(全 JAR / 全指针)额外显示专属操作区.
class BatchEditorPanel : public QWidget {
    Q_OBJECT

public:
    explicit BatchEditorPanel(QWidget* parent = nullptr);

    void loadSelection(const QList<HiBerGUI::RepoObjectInfo>& infos);
    void clearSelection();
    void setHasParser(bool hasParser);

signals:
    void batchPolicySaveRequested(const QStringList& paths, const QString& mode,
        const std::vector<std::string>& trackedKeys,
        const std::vector<int>& trackedLines, bool toBranch);
    void batchConvertJarsRequested(const QStringList& relPaths);
    void batchRestorePointersRequested(
        const QList<HiBerGUI::RepoObjectInfo>& infos);
    void batchDeleteRequested(const QList<HiBerGUI::RepoObjectInfo>& infos);
    void contentModified();

private:
    QLabel* titleLabel_;
    QListWidget* selectionList_;
    QGroupBox* policyGroup_;
    QComboBox* modeCombo_;
    QPlainTextEdit* keysEdit_;
    QLineEdit* linesEdit_;
    QRadioButton* topRb_;
    QRadioButton* branchRb_;
    QPushButton* applyButton_;
    QGroupBox* jarGroup_;
    QPushButton* convertButton_;
    QGroupBox* pointerGroup_;
    QPushButton* restoreButton_;
    QPushButton* deleteButton_;

    QList<HiBerGUI::RepoObjectInfo> selection_;
    bool hasParser_ = true;

    void applyPolicy();
    void updateSections();
    void updatePartialEnabled();
    void applyStyle();
};

} // namespace GUIWorker
