#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <nlohmann/json.hpp>

#include "sync_policies_editor.h"

namespace GUIWorker {

class RepoEditor : public QWidget {
    Q_OBJECT

public:
    explicit RepoEditor(QWidget* parent = nullptr);

    void loadFromJson(const nlohmann::json& config);
    nlohmann::json saveToJson() const;

    bool isModified() const;

signals:
    void contentModified();
    void saveRequested(const QString& jsonString);
    void connectionTestClicked(const QString& url);

private slots:
    void onTestConnection();
    void onCellChanged();

private:
    void buildUI();
    void connectSignals();
    void markModified();

    QLabel* titleLabel_;

    QGroupBox* workspaceGroup_;
    QLineEdit* nameEdit_;
    QLineEdit* minecraftEdit_;
    QComboBox* modloaderCombo_;
    QLineEdit* modloaderVersionEdit_;

    QGroupBox* gitGroup_;
    QLineEdit* remoteEdit_;
    QLineEdit* defaultBranchEdit_;
    QPushButton* testBtn_;
    QLabel* testResult_;

    QGroupBox* syncPoliciesGroup_;
    SyncPoliciesEditor* syncPoliciesEditor_;

    QGroupBox* customModsGroup_;
    QLineEdit* customModsPathEdit_;

    QPushButton* saveBtn_;

    bool modified_;
};

} // namespace GUIWorker
