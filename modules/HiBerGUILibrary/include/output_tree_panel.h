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

    void loadEntries(const nlohmann::json& entries,
        const QSet<QString>& pointerRels = {});
    // 额外视为配置文件的相对路径集合 (用户标记), 参与 ConfigFile 类型判定
    void setExtraConfigFiles(const QSet<QString>& rels);
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
    void objectActivated(const RepoObjectInfo& info);
    void createServerConfigRequested();
    // 普通文件 ↔ 配置文件标记 (持久化由宿主完成)
    void markAsConfigFileRequested(const RepoObjectInfo& info);
    void unmarkConfigFileRequested(const RepoObjectInfo& info);
    // 拖放悬停时指示落点 (hovering=false 表示拖放结束/离开)
    void dropTargetChanged(const QString& targetRel, bool hovering);

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
    QSet<QString> extraConfigFiles_;

    // 拖放悬停高亮 (与选中色区分)
    QTreeWidgetItem* dropHighlightItem_ = nullptr;
    QBrush dropHighlightOldBrush_;
    bool dropHighlightHadBrush_ = false;

    QString relPathAt(const QPoint& pos) const;
    RepoObjectInfo infoFromItem(QTreeWidgetItem* item) const;
    QString clipTarget(const RepoObjectInfo& cur) const;
    bool confirmBatchDelete(int count);
    void applyStyle();
    void setDropHighlight(QTreeWidgetItem* item);
    void clearDropHighlight();
};

} // namespace HiBerGUI
