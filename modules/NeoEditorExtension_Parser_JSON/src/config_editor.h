#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QStringList>
#include <QJsonObject>
#include <string>

class ConfigEditor : public QWidget {
public:
    explicit ConfigEditor(QWidget* parent = nullptr);

    void setContent(const std::string& content);
    void setSyncMode(const std::string& mode);
    std::string syncMode() const;
    void setKeys(const QStringList& keys, const QStringList& tracked);
    QStringList trackedKeys() const;
    void setLineMode(bool lineMode);
    std::string mergePreview() const;
    void setTrackedLines(const QList<int>& lines);
    QList<int> trackedLines() const;

private:
    QTextEdit* contentView_;
    QComboBox* modeCombo_;
    QTreeWidget* keyTree_;
    QPushButton* toggleAllBtn_;
    bool lineMode_ = false;
};
