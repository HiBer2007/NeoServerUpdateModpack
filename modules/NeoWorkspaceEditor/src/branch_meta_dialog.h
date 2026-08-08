#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <string>
#include <vector>

class BranchMetaDialog : public QDialog {
    Q_OBJECT

public:
    explicit BranchMetaDialog(const QString& repoDir, QWidget* parent = nullptr);

private slots:
    void onBranchSelected();
    void onSave();

private:
    struct BranchMeta {
        QString name;
        bool    isDefault = false;
        bool    current = false;
        bool    local = false;
        QString description;
        bool    hidden = false;
    };

    bool loadMeta(BranchMeta& meta);
    void refreshList();
    bool runGit(const QStringList& args, QString* out = nullptr, QString* err = nullptr);

    QString repoDir_;

    QListWidget* list_;
    QLineEdit* descEdit_;
    QCheckBox* hiddenCheck_;
    QLabel* infoLabel_;
    QPushButton* saveBtn_;

    std::vector<BranchMeta> branches_;
    int currentIndex_ = -1;
};
