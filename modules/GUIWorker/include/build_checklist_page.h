#pragma once

#include <QWidget>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>
#include <QString>
#include <QMap>
#include <nlohmann/json.hpp>

namespace GUIWorker {

class CollapsibleSection : public QWidget {
    Q_OBJECT

public:
    explicit CollapsibleSection(const QString& title, QWidget* content,
                                QWidget* parent = nullptr);

    void setExpanded(bool expanded);
    bool isExpanded() const;
    void toggle();
    QWidget* content() const { return content_; }
    int headerHeight() const { return headerBtn_->sizeHint().height(); }
    int contentHeight(int widthHint) const;

signals:
    void expandedChanged();

private:
    QPushButton* headerBtn_;
    QWidget* content_;
    bool expanded_;
};

class BuildChecklistPage : public QWidget {
    Q_OBJECT

public:
    explicit BuildChecklistPage(QWidget* parent = nullptr);

    void setSummary(const QString& key, const QString& value);
    void clearSummary();
    void setExtraInfo(const QMap<QString, QString>& values);
    void loadStructure(const nlohmann::json& entries);
    void clearFileTree();
    void expandAll();
    void collapseAll();
    int  contentHeight(int widthHint) const;

signals:
    void treeExpandedChanged();

private:
    QLabel* summaryText_;
    QTreeWidget* fileTree_;
    QLabel* statusLabel_;
    QLabel* extraText_;
    CollapsibleSection* configSection_;
    CollapsibleSection* extraSection_;
    CollapsibleSection* treeSection_;
    QPushButton* expandBtn_;
    QPushButton* collapseBtn_;
    QMap<QString, QString> extraValues_;
};

} // namespace GUIWorker
