#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QCheckBox>
#include <QLabel>
#include <QGroupBox>
#include <nlohmann/json.hpp>

#include "sync_policies_editor.h"

namespace GUIWorker {

class BranchEditor : public QWidget {
    Q_OBJECT

public:
    explicit BranchEditor(QWidget* parent = nullptr);

    void loadBranches(const std::vector<nlohmann::json>& branches,
        const std::string& defaultBranch = "");
    std::vector<nlohmann::json> saveBranches() const;

    bool isModified() const;

signals:
    void contentModified();
    void saveRequested(QString jsonString);

private slots:
    void onAddBranch();
    void onRemoveBranch();
    void onBranchSelected();

    void onNameChanged(const QString& text);
    void onParentChanged(int index);
    void onModloaderVersionChanged(const QString& text);
    void onDescriptionChanged(const QString& text);
    void onHiddenToggled(bool checked);
    void onEditSyncPolicies();

    void refreshParentCombos();
    void rebuildInheritanceTree();

private:
    void buildUI();
    void connectSignals();
    void syncTableToCurrentBranch();
    void syncCurrentBranchToTable();
    void validateBranchConfig();
    void updatePoliciesSummary();
    void markModified();

    struct BranchRow {
        std::string name;
        std::string parent;
        std::string modloaderVersion;
        std::string description;
        bool hidden = false;
        nlohmann::json raw;
    };

    std::vector<BranchRow> branches_;
    int currentIndex_;

    QLabel* titleLabel_;

    QGroupBox* listGroup_;
    QTableWidget* branchTable_;
    QPushButton* addBranchBtn_;
    QPushButton* removeBranchBtn_;

    QGroupBox* editorGroup_;
    QLineEdit* nameEdit_;
    QComboBox* parentCombo_;
    QLineEdit* modloaderVersionEdit_;
    QLineEdit* descriptionEdit_;
    QCheckBox* hiddenCheck_;

    QGroupBox* syncPoliciesGroup_;
    QLabel* spInfoLabel_;
    QPushButton* editPoliciesBtn_;
    QLabel* policiesSummary_;

    QGroupBox* inheritanceGroup_;
    QTreeWidget* inheritanceTree_;

    QPushButton* saveBtn_;
    QLabel* warningLabel_;

    bool modified_;
};

} // namespace GUIWorker
