#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QDir>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <QDateTime>

#include <string>

#include <QPushButton>

namespace HiBerGUI {

enum class RepoObjectType {
    Root,
    Folder,
    ConfigFile,
    PlainFile,
    Pointer,
};

struct RepoObjectInfo {
    RepoObjectType type = RepoObjectType::Root;
    QString path;
    QString displayName;
    QString pointerSha;
    bool isInherited = false;
    QString marker;  // "delete" / "override" / 空
};

// Prompts for a folder name and creates it under absParent.
// Returns true if a folder was created.
bool createFolderInteractive(QWidget* parent, const QString& absParent);

class RepoTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit RepoTreePanel(QWidget* parent = nullptr);

    void setRootPath(const QString& branchDir);
    void setPointerDir(const QString& branchConfigDir);
    // rebuild=false 时仅更新数据不重建树 (宿主统一重建, 避免多次全量刷新)
    void setInheritedFiles(const QStringList& rels, bool rebuild = true);
    void setBranchManifest(const QMap<QString, QString>& markers, bool rebuild = true);
    // 指针文件按真实位置显示 (rel → sha256), resolver 用于提示
    void setPointerFiles(const QMap<QString, QString>& relToSha,
        const QMap<QString, QString>& relToResolver, bool rebuild = true);
    // 额外视为配置文件的相对路径集合 (用户标记), 参与 ConfigFile 类型判定
    void setExtraConfigFiles(const QSet<QString>& rels);
    void refresh();
    QString rootPath() const { return rootPath_; }
    QString pointerDir() const { return pointerDir_; }
    QTreeWidget* tree() const { return tree_; }
    RepoObjectInfo currentSelection() const;
    QList<RepoObjectInfo> selectedObjects() const;

signals:
    void objectActivated(const RepoObjectInfo& info);
    void batchConvertJarsRequested(const QString& folderPath);
    void convertToPointerRequested(const RepoObjectInfo& info);
    void restorePointerRequested(const QString& sha);
    void filesDropped(const QStringList& filePaths, const QString& targetFolderRel);
    void deleteRequested(const RepoObjectInfo& info);
    void batchDeleteRequested(const QList<RepoObjectInfo>& infos);
    void restoreInheritedRequested(const RepoObjectInfo& info);
    void contentEditRequested(const RepoObjectInfo& info);
    void folderPolicyEditRequested(const RepoObjectInfo& info);
    void filePolicyEditRequested(const RepoObjectInfo& info);
    void importOverwriteRequested(const RepoObjectInfo& info);
    void copyRequested(const QList<RepoObjectInfo>& infos);
    void cutRequested(const QList<RepoObjectInfo>& infos);
    void pasteRequested(const QStringList& relPaths, const QString& targetRel,
        bool isCut);
    void batchRestorePointersRequested(const QList<RepoObjectInfo>& infos);
    void moveItemsRequested(const QList<RepoObjectInfo>& infos,
        const QString& targetRel, bool copy);
    void createServerConfigRequested();
    // 普通文件 ↔ 配置文件标记 (持久化由宿主完成)
    void markAsConfigFileRequested(const RepoObjectInfo& info);
    void unmarkConfigFileRequested(const RepoObjectInfo& info);
    // 拖放悬停时指示落点 (hovering=false 表示拖放结束/离开)
    void dropTargetChanged(const QString& targetRel, bool hovering);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QTreeWidget* tree_;
    QLabel* statusLabel_;
    QString rootPath_;
    QString pointerDir_;
    QStringList inheritedFiles_;
    QMap<QString, QString> branchMarkers_;
    QMap<QString, QString> pointerFiles_;
    QMap<QString, QString> pointerResolvers_;
    QSet<QString> extraConfigFiles_;

    QStringList clipPaths_;
    bool clipIsCut_ = false;
    QPoint dragStartPos_;

    // 拖放悬停高亮 (与选中色区分)
    QTreeWidgetItem* dropHighlightItem_ = nullptr;
    QBrush dropHighlightOldBrush_;
    bool dropHighlightHadBrush_ = false;

    // 重建前保存的展开路径 (dirRel), 重建后恢复展开, 避免刷新白展开
    QSet<QString> expandedPaths_;

    void rebuildTree();
    void collectExpandedPaths(QTreeWidgetItem* item, const QString& prefix);
    void restoreExpandedPaths(QTreeWidgetItem* item, const QString& prefix);
    void addDirectoryTree(QTreeWidgetItem* parent, const QDir& dir,
        const QString& dirRel);
    void addVirtualChildren(QTreeWidgetItem* parent, const QString& dirRel);
    void addPointerChildren(QTreeWidgetItem* parent, const QString& dirRel);
    int addPointerGroup();
    void applyStyle();
    void createNewFolder(const QString& parentRel);
    bool confirmBatchDelete(int count);
    QString clipTarget(const RepoObjectInfo& cur) const;
    QString buildObjectPath(QTreeWidgetItem* item) const;
    RepoObjectInfo infoFromItem(QTreeWidgetItem* item) const;
    QString targetRelAt(const QPoint& pos) const;
    void setDropHighlight(QTreeWidgetItem* item);
    void clearDropHighlight();
};

} // namespace HiBerGUI
