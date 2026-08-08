#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QDir>
#include <QStringList>
#include <QMap>

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

class RepoTreePanel : public QWidget {
    Q_OBJECT

public:
    explicit RepoTreePanel(QWidget* parent = nullptr);

    void setRootPath(const QString& branchDir);
    void setPointerDir(const QString& branchConfigDir);
    void setInheritedFiles(const QStringList& rels);
    void setBranchManifest(const QMap<QString, QString>& markers);
    void refresh();
    QString rootPath() const { return rootPath_; }
    QString pointerDir() const { return pointerDir_; }
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

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
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

    QStringList clipPaths_;
    bool clipIsCut_ = false;
    QPoint dragStartPos_;

    void rebuildTree();
    void addDirectoryTree(QTreeWidgetItem* parent, const QDir& dir,
        const QString& dirRel);
    void addVirtualChildren(QTreeWidgetItem* parent, const QString& dirRel);
    int addPointerGroup();
    void applyStyle();
    void createNewFolder(const QString& parentRel);
    bool confirmBatchDelete(int count);
    QString clipTarget(const RepoObjectInfo& cur) const;
    QString buildObjectPath(QTreeWidgetItem* item) const;
    RepoObjectInfo infoFromItem(QTreeWidgetItem* item) const;
};

} // namespace HiBerGUI
