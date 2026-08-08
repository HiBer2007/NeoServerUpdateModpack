#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QJsonObject>

class ModrinthEditor : public QWidget {
public:
    explicit ModrinthEditor(QWidget* parent = nullptr);

    void loadMetadata(const QJsonObject& metadata);
    QJsonObject saveMetadata() const;

private:
    QLineEdit* projectIdEdit_;
    QLineEdit* versionIdEdit_;
    QLineEdit* modNameEdit_;
    QPushButton* fetchBtn_;
    QLabel* statusLabel_;
};
