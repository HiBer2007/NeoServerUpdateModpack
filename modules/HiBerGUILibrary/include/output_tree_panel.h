#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QEvent>

#include <nlohmann/json.hpp>

#include "repo_tree_panel.h"

namespace HiBerGUI {

class OutputTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit OutputTreePanel(QWidget* parent = nullptr);

    void loadEntries(const nlohmann::json& entries);
    void setStatusText(const QString& text);
    void setFormat(const QString& format);
    QString format() const;
    QTreeWidget* tree() const { return tree_; }
    RepoObjectInfo currentSelection() const;
    QList<RepoObjectInfo> selectedObjects() const;

    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void formatChanged(const QString& format);
    void refreshRequested();
    void filesDropped(const QStringList& filePaths, const QString& targetRel);
    void deleteRequested(const RepoObjectInfo& info);
    void batchDeleteRequested(const QList<RepoObjectInfo>& infos);
    void pasteRequested(const QStringList& relPaths, const QString& targetRel,
        bool isCut);
    void newFolderRequested(const QString& parentRel);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QTreeWidget* tree_;
    QLabel* statusLabel_;
    QComboBox* formatCombo_;
    QPushButton* refreshButton_;

    QStringList clipPaths_;
    bool clipIsCut_ = false;

    QString relPathAt(const QPoint& pos) const;
    RepoObjectInfo infoFromItem(QTreeWidgetItem* item) const;
    QString clipTarget(const RepoObjectInfo& cur) const;
    bool confirmBatchDelete(int count);
    void applyStyle();
};

} // namespace HiBerGUI
