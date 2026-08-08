#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QDialog>
#include <QStringList>

#include <nlohmann/json.hpp>

namespace GUIWorker {

class SyncPoliciesEditor : public QWidget {
    Q_OBJECT

public:
    explicit SyncPoliciesEditor(QWidget* parent = nullptr);

    void load(const nlohmann::json& policies);
    nlohmann::json save() const;
    void setFolderCandidates(const QStringList& dirs);

signals:
    void contentModified();

private slots:
    void onAddFolderRow();
    void onRemoveFolderRow();
    void onAddFileRow();
    void onRemoveFileRow();

private:
    void buildUI();
    void emitModified();

    nlohmann::json original_;

    QComboBox* defaultPolicyCombo_;
    QTableWidget* folderTable_;
    QTableWidget* fileTable_;
    QPushButton* addFolderBtn_;
    QPushButton* removeFolderBtn_;
    QPushButton* addFileBtn_;
    QPushButton* removeFileBtn_;
};

class SyncPoliciesDialog : public QDialog {
    Q_OBJECT

public:
    explicit SyncPoliciesDialog(QWidget* parent = nullptr);

    void load(const nlohmann::json& policies);
    nlohmann::json save() const;

private:
    SyncPoliciesEditor* editor_;
};

} // namespace GUIWorker
