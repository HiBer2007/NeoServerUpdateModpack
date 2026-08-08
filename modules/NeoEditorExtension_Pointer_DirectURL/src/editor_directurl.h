#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QJsonObject>

class DirectURLEditor : public QWidget {
public:
    explicit DirectURLEditor(QWidget* parent = nullptr);

    void loadMetadata(const QJsonObject& metadata);
    QJsonObject saveMetadata() const;

private:
    QLineEdit* urlEdit_;
    QLineEdit* filenameEdit_;
};
