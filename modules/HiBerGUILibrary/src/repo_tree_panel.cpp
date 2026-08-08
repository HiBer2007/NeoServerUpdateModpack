#include "repo_tree_panel.h"

#include <QVBoxLayout>
#include <QDir>
#include <QFileInfo>
#include <QBrush>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QFile>
#include <QMimeData>
#include <QUrl>
#include <QSet>
#include <QInputDialog>
#include <QMessageBox>
#include <QMouseEvent>
#include <QDrag>
#include <QApplication>
#include <QMimeData>

#include <nlohmann/json.hpp>

namespace HiBerGUI {

namespace {

bool isConfigFile(const QString& name)
{
    const QString lower = name.toLower();
    return lower.endsWith(QStringLiteral(".json"))
        || lower.endsWith(QStringLiteral(".yaml"))
        || lower.endsWith(QStringLiteral(".yml"))
        || lower.endsWith(QStringLiteral(".toml"))
        || lower.endsWith(QStringLiteral(".snbt"))
        || lower.endsWith(QStringLiteral(".txt"))
        || lower.endsWith(QStringLiteral(".properties"));
}

bool isTextEditable(const QString& name)
{
    const QString lower = name.toLower();
    static const QStringList exts = {
        QStringLiteral("json"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("toml"), QStringLiteral("snbt"), QStringLiteral("txt"),
        QStringLiteral("properties"), QStringLiteral("cfg"), QStringLiteral("conf"),
        QStringLiteral("ini"), QStringLiteral("md"), QStringLiteral("log"),
        QStringLiteral("xml"), QStringLiteral("mcmeta"), QStringLiteral("lang"),
    };
    for (const QString& e : exts) {
        if (lower.endsWith(QLatin1Char('.') + e)) return true;
    }
    return false;
}

QString originalNameFromPointer(const QString& pointerPath)
{
    QFile f(pointerPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    try {
        auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        const auto& names = j.value("original_names", nlohmann::json::array());
        if (names.is_array() && !names.empty()) {
            return QString::fromStdString(names[0].get<std::string>());
        }
        const std::string sha = j.value("sha256", std::string());
        if (!sha.empty()) {
            return QString::fromStdString(sha).left(16);
        }
    } catch (...) {
        f.close();
    }
    return QString();
}

} // namespace

RepoTreePanel::RepoTreePanel(QWidget* parent)
    : QWidget(parent)
{
    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({ QString::fromUtf8("\u6587\u4ef6"),
        QString::fromUtf8("\u7c7b\u578b") });
    tree_->setColumnWidth(0, 200);
    tree_->setAlternatingRowColors(true);
    tree_->setAnimated(true);
    tree_->setAcceptDrops(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->installEventFilter(this);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("repoTreeStatus"));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    lay->addWidget(tree_, 1);
    lay->addWidget(statusLabel_);

    connect(tree_, &QTreeWidget::itemClicked, this,
        [this](QTreeWidgetItem* item, int column) {
            if (!item) return;
            emit objectActivated(infoFromItem(item));
            Q_UNUSED(column);
        });

    applyStyle();
}

void RepoTreePanel::setRootPath(const QString& branchDir)
{
    rootPath_ = branchDir;
    refresh();
}

void RepoTreePanel::setPointerDir(const QString& branchConfigDir)
{
    pointerDir_ = branchConfigDir;
    refresh();
}

void RepoTreePanel::setInheritedFiles(const QStringList& rels)
{
    inheritedFiles_ = rels;
    refresh();
}

void RepoTreePanel::setBranchManifest(const QMap<QString, QString>& markers)
{
    branchMarkers_ = markers;
    refresh();
}

void RepoTreePanel::refresh()
{
    rebuildTree();
}

RepoObjectInfo RepoTreePanel::currentSelection() const
{
    return infoFromItem(tree_->currentItem());
}

QList<RepoObjectInfo> RepoTreePanel::selectedObjects() const
{
    QList<RepoObjectInfo> result;
    const auto items = tree_->selectedItems();
    for (QTreeWidgetItem* item : items) {
        if (!item) continue;
        const RepoObjectInfo info = infoFromItem(item);
        if (info.type == RepoObjectType::Root) continue;
        if (info.type == RepoObjectType::Pointer) {
            if (!info.pointerSha.isEmpty()) {
                result << info;
            }
            continue;
        }
        if (!info.path.isEmpty()) {
            result << info;
        }
    }
    return result;
}

RepoObjectInfo RepoTreePanel::infoFromItem(QTreeWidgetItem* item) const
{
    RepoObjectInfo info;
    if (!item) return info;
    info.path = buildObjectPath(item);
    info.displayName = item->text(0);
    if (info.path == QStringLiteral(".")) {
        info.type = RepoObjectType::Root;
        return info;
    }
    const QString marker = item->data(0, Qt::UserRole).toString();
    if (marker == QLatin1String("pointer")) {
        info.type = RepoObjectType::Pointer;
        info.pointerSha = item->data(0, Qt::UserRole + 1).toString();
        return info;
    }
    if (marker == QLatin1String("inherited")
        || marker == QLatin1String("deleted")) {
        info.isInherited = true;
        info.marker = (marker == QLatin1String("deleted"))
            ? QStringLiteral("delete") : QString();
        info.type = item->childCount() > 0
            ? RepoObjectType::Folder : RepoObjectType::PlainFile;
        return info;
    }
    const QString selfMarker = item->data(0, Qt::UserRole + 2).toString();
    info.marker = selfMarker;
    if (item->childCount() > 0) {
        info.type = RepoObjectType::Folder;
    } else if (isConfigFile(item->text(0))) {
        info.type = RepoObjectType::ConfigFile;
    } else {
        info.type = RepoObjectType::PlainFile;
    }
    return info;
}

void RepoTreePanel::rebuildTree()
{
    tree_->clear();
    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    if (rootPath_.isEmpty()) {
        statusLabel_->setText(QString::fromUtf8("\u672a\u6253\u5f00\u4ed3\u5e93\u3002"));
        return;
    }

    QDir dir(rootPath_);
    if (!dir.exists()) {
        statusLabel_->setText(QString::fromUtf8("\u76ee\u5f55\u4e0d\u5b58\u5728:\n%1").arg(rootPath_));
        return;
    }

    auto* rootItem = new QTreeWidgetItem(tree_);
    rootItem->setText(0, dir.dirName().isEmpty()
        ? QStringLiteral(".") : dir.dirName());
    rootItem->setText(1, QString::fromUtf8("\u6570\u636e\u76ee\u5f55"));
    addDirectoryTree(rootItem, dir, QString());
    addVirtualChildren(rootItem, QString());

    int fileCount = 0;
    std::function<void(QTreeWidgetItem*)> countFiles = [&](QTreeWidgetItem* item) {
        for (int i = 0; i < item->childCount(); ++i) {
            QTreeWidgetItem* child = item->child(i);
            if (child->childCount() > 0) {
                countFiles(child);
            } else {
                ++fileCount;
            }
        }
    };
    countFiles(rootItem);

    const int pointerCount = addPointerGroup();
    rootItem->setExpanded(true);

    QString stat = QString::fromUtf8("%1 \u4e2a\u6587\u4ef6\uff0c%2 \u4e2a\u6307\u9488\u3002")
        .arg(fileCount).arg(pointerCount);
    statusLabel_->setText(stat);
    Q_UNUSED(darkMode);
}

void RepoTreePanel::addDirectoryTree(QTreeWidgetItem* parent, const QDir& dir,
    const QString& dirRel)
{
    static const QStringList skipDirs = {
        QStringLiteral(".git"), QStringLiteral(".NSUM"),
        QStringLiteral(".rule"), QStringLiteral("branch_config"),
        QStringLiteral(".overrides"),
    };

    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::Files
        | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);
    for (const auto& fi : entries) {
        const QString name = fi.fileName();
        if (fi.isDir()) {
            if (skipDirs.contains(name)) continue;
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setText(1, QString::fromUtf8("\u76ee\u5f55"));
            const QString rel = dirRel.isEmpty()
                ? name : dirRel + QLatin1Char('/') + name;
            addDirectoryTree(item, QDir(fi.absoluteFilePath()), rel);
            addVirtualChildren(item, rel);
        } else {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setText(1, isConfigFile(name)
                ? QString::fromUtf8("\u914d\u7f6e")
                : QString::fromUtf8("\u6587\u4ef6"));
            const QString rel = dirRel.isEmpty()
                ? name : dirRel + QLatin1Char('/') + name;
            const QString m = branchMarkers_.value(rel);
            if (!m.isEmpty()) {
                item->setData(0, Qt::UserRole + 2, m);
                if (m == QLatin1String("override")) {
                    item->setText(1, QString::fromUtf8("\u8986\u76d6"));
                }
            }
        }
    }
}

void RepoTreePanel::addVirtualChildren(QTreeWidgetItem* parent,
    const QString& dirRel)
{
    const QColor windowBg = palette().color(QPalette::Window);
    const bool dark = windowBg.lightness() < 128;
    const QColor inhColor = dark
        ? QColor(QStringLiteral("#4dd0e1")) : QColor(QStringLiteral("#0097a7"));
    const QColor delColor = dark
        ? QColor(QStringLiteral("#e57373")) : QColor(QStringLiteral("#c62828"));

    const QString prefix = dirRel.isEmpty() ? QString() : dirRel + QLatin1Char('/');
    const QString dirAbs = rootPath_ + (dirRel.isEmpty()
        ? QString() : QLatin1Char('/') + dirRel);

    QSet<QString> dirs, files, dels;
    for (const QString& rel : inheritedFiles_) {
        if (!rel.startsWith(prefix)) continue;
        const QString rest = prefix.isEmpty() ? rel : rel.mid(prefix.size());
        if (rest.isEmpty()) continue;
        const int slash = rest.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            dirs.insert(rest.left(slash));
        } else {
            files.insert(rest);
        }
    }
    for (auto it = branchMarkers_.begin(); it != branchMarkers_.end(); ++it) {
        if (it.value() != QLatin1String("delete")) continue;
        const QString& rel = it.key();
        if (!rel.startsWith(prefix)) continue;
        const QString rest = prefix.isEmpty() ? rel : rel.mid(prefix.size());
        if (rest.isEmpty()) continue;
        const int slash = rest.indexOf(QLatin1Char('/'));
        if (slash >= 0) {
            dirs.insert(rest.left(slash));
        } else {
            dels.insert(rest);
        }
    }

    for (const QString& name : dirs) {
        if (QFileInfo::exists(dirAbs + QLatin1Char('/') + name)) continue;
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, QString::fromUtf8("\u7ee7\u627f"));
        item->setForeground(0, QBrush(inhColor));
        item->setForeground(1, QBrush(inhColor));
        item->setData(0, Qt::UserRole, QStringLiteral("inherited"));
        addVirtualChildren(item,
            dirRel.isEmpty() ? name : dirRel + QLatin1Char('/') + name);
    }
    for (const QString& name : files) {
        if (QFileInfo::exists(dirAbs + QLatin1Char('/') + name)) continue;
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, QString::fromUtf8("\u7ee7\u627f"));
        item->setForeground(0, QBrush(inhColor));
        item->setForeground(1, QBrush(inhColor));
        item->setData(0, Qt::UserRole, QStringLiteral("inherited"));
    }
    for (const QString& name : dels) {
        if (QFileInfo::exists(dirAbs + QLatin1Char('/') + name)) continue;
        auto* item = new QTreeWidgetItem(parent);
        item->setText(0, name);
        item->setText(1, QString::fromUtf8("\u2718 \u5df2\u5220\u9664"));
        item->setForeground(0, QBrush(delColor));
        item->setForeground(1, QBrush(delColor));
        item->setData(0, Qt::UserRole, QStringLiteral("deleted"));
    }
}

int RepoTreePanel::addPointerGroup()
{
    if (pointerDir_.isEmpty()) return 0;

    QDir dir(pointerDir_);
    if (!dir.exists()) return 0;

    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;
    const QColor pointerColor = darkMode
        ? QColor(QStringLiteral("#4dd0e1"))
        : QColor(QStringLiteral("#0097a7"));

    int count = 0;
    const auto pointers = dir.entryInfoList({ QStringLiteral("*.pointer") },
        QDir::Files, QDir::Name);
    if (pointers.isEmpty()) return 0;

    auto* group = new QTreeWidgetItem(tree_);
    group->setText(0, QString::fromUtf8("\u6307\u9488\u6587\u4ef6 (%1)")
        .arg(pointers.size()));
    group->setText(1, QString::fromUtf8("\u7ec4"));
    group->setForeground(0, QBrush(pointerColor));

    for (const auto& fi : pointers) {
        const QString sha = fi.completeBaseName();
        const QString original = originalNameFromPointer(fi.absoluteFilePath());
        auto* item = new QTreeWidgetItem(group);
        if (!original.isEmpty()) {
            item->setText(0, original);
            item->setText(1, QString::fromUtf8("\u6307\u9488"));
        } else {
            item->setText(0, fi.fileName());
            item->setText(1, QString::fromUtf8("\u6307\u9488"));
        }
        item->setForeground(0, QBrush(pointerColor));
        item->setForeground(1, QBrush(pointerColor));
        item->setData(0, Qt::UserRole, QStringLiteral("pointer"));
        item->setData(0, Qt::UserRole + 1, sha);
        ++count;
    }

    group->setExpanded(true);
    return count;
}

QString RepoTreePanel::buildObjectPath(QTreeWidgetItem* item) const
{
    if (!item) return QString();
    QStringList parts;
    QTreeWidgetItem* cur = item;
    while (cur && cur->parent()) {
        parts.prepend(cur->text(0));
        cur = cur->parent();
    }
    if (parts.isEmpty()) return QStringLiteral(".");
    return parts.join(QLatin1Char('/'));
}

void RepoTreePanel::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }
    if (event->mimeData()->hasFormat(QStringLiteral("application/x-nsum-repo-items"))) {
        event->acceptProposedAction();
        return;
    }
    QWidget::dragEnterEvent(event);
}

void RepoTreePanel::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-nsum-repo-items"))) {
        QByteArray data = event->mimeData()->data(
            QStringLiteral("application/x-nsum-repo-items"));
        QStringList rels = QString::fromUtf8(data).split(
            QLatin1Char('\n'), Qt::SkipEmptyParts);
        QTreeWidgetItem* item = tree_->itemAt(event->position().toPoint());
        QString targetRel;
        if (item) {
            const QString marker = item->data(0, Qt::UserRole).toString();
            if (marker != QLatin1String("pointer")) {
                const QString p = buildObjectPath(item);
                if (p != QStringLiteral(".")) {
                    targetRel = (item->childCount() > 0) ? p
                        : QFileInfo(p).path().replace(QLatin1Char('\\'), QLatin1Char('/'));
                }
            }
        }
        const bool copy = (event->keyboardModifiers()
            & Qt::ControlModifier) != 0;
        event->acceptProposedAction();

        QList<RepoObjectInfo> infos;
        for (const QString& rel : rels) {
            if (rel.isEmpty() || rel == QStringLiteral(".")) continue;
            RepoObjectInfo info;
            info.path = rel;
            info.type = RepoObjectType::PlainFile;
            infos << info;
        }
        if (!infos.isEmpty()) {
            emit moveItemsRequested(infos, targetRel, copy);
        }
        return;
    }
    if (!event->mimeData()->hasUrls()) {
        QWidget::dropEvent(event);
        return;
    }
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        const QString p = url.toLocalFile();
        if (QFileInfo::exists(p)) {
            paths << p;
        }
    }
    if (paths.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();

    QTreeWidgetItem* item = tree_->itemAt(event->position().toPoint());
    QString targetRel;
    if (item) {
        const QString marker = item->data(0, Qt::UserRole).toString();
        if (marker != QLatin1String("pointer")) {
            const QString p = buildObjectPath(item);
            if (p != QStringLiteral(".")) {
                targetRel = (item->childCount() > 0) ? p
                    : QFileInfo(p).path().replace(QLatin1Char('\\'), QLatin1Char('/'));
            }
        }
    }
    emit filesDropped(paths, targetRel);
}

void RepoTreePanel::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = tree_->itemAt(event->pos());
    if (!item) return;

    const auto sel = selectedObjects();
    if (sel.size() > 1) {
        QMenu menu(this);
        bool anyPointer = false;
        for (const auto& info : sel) {
            if (info.type == RepoObjectType::Pointer) {
                anyPointer = true;
                break;
            }
        }
        if (anyPointer) {
            QList<RepoObjectInfo> pointers;
            for (const auto& info : sel) {
                if (info.type == RepoObjectType::Pointer) {
                    pointers << info;
                }
            }
            QAction* restorePtrAction = menu.addAction(
                QString::fromUtf8("\u6279\u91cf\u8f6c\u56de\u539f\u59cb\u6587\u4ef6 (%1)")
                    .arg(pointers.size()));
            connect(restorePtrAction, &QAction::triggered, this,
                [this, pointers]() {
                    emit batchRestorePointersRequested(pointers);
                });
            menu.addSeparator();
        }
        QAction* copyAction = menu.addAction(
            QString::fromUtf8("\u590d\u5236 (%1)").arg(sel.size()));
        connect(copyAction, &QAction::triggered, this, [this]() {
            const auto s = selectedObjects();
            QStringList paths;
            for (const auto& info : s) {
                if (info.type == RepoObjectType::Pointer) continue;
                paths << info.path;
            }
            clipPaths_ = paths;
            clipIsCut_ = false;
        });
        QAction* cutAction = menu.addAction(
            QString::fromUtf8("\u526a\u5207 (%1)").arg(sel.size()));
        connect(cutAction, &QAction::triggered, this, [this]() {
            const auto s = selectedObjects();
            QStringList paths;
            for (const auto& info : s) {
                if (info.type == RepoObjectType::Pointer) continue;
                paths << info.path;
            }
            clipPaths_ = paths;
            clipIsCut_ = true;
        });
        menu.addSeparator();
        QAction* delAction = menu.addAction(
            QString::fromUtf8("\u5220\u9664 %1 \u4e2a\u9879\u76ee...").arg(sel.size()));
        connect(delAction, &QAction::triggered, this, [this, sel]() {
            QList<RepoObjectInfo> deletable;
            for (const auto& info : sel) {
                if (info.type != RepoObjectType::Pointer) {
                    deletable << info;
                }
            }
            if (deletable.isEmpty()) return;
            if (confirmBatchDelete(deletable.size())) {
                emit batchDeleteRequested(deletable);
            }
        });
        menu.exec(event->globalPos());
        return;
    }

    const QString marker = item->data(0, Qt::UserRole).toString();
    if (marker == QLatin1String("pointer")) {
        QMenu menu(this);
        QAction* restoreAction = menu.addAction(
            QString::fromUtf8("\u8f6c\u56de\u539f\u59cb\u6587\u4ef6"));
        connect(restoreAction, &QAction::triggered, this, [this, item]() {
            emit restorePointerRequested(
                item->data(0, Qt::UserRole + 1).toString());
        });
        menu.exec(event->globalPos());
        return;
    }

    const RepoObjectInfo info = infoFromItem(item);
    if (info.type == RepoObjectType::Root) {
        QMenu menu(this);
        QAction* newFolderAction = menu.addAction(
            QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939..."));
        connect(newFolderAction, &QAction::triggered, this, [this]() {
            createNewFolder(QString());
        });
        if (!clipPaths_.isEmpty()) {
            menu.addSeparator();
            QAction* pasteAction = menu.addAction(
                clipIsCut_ ? QString::fromUtf8("\u7c98\u8d34 (\u526a\u5207)")
                           : QString::fromUtf8("\u7c98\u8d34"));
            connect(pasteAction, &QAction::triggered, this, [this]() {
                emit pasteRequested(clipPaths_, QString(), clipIsCut_);
            });
        }
        menu.exec(event->globalPos());
        return;
    }

    if (info.marker == QLatin1String("delete")) {
        QMenu menu(this);
        QAction* restoreAction = menu.addAction(
            QString::fromUtf8("\u8fd8\u539f\u7236\u7248\u672c"));
        connect(restoreAction, &QAction::triggered, this, [this, info]() {
            emit restoreInheritedRequested(info);
        });
        menu.exec(event->globalPos());
        return;
    }

    if (item->childCount() > 0) {
        QMenu menu(this);
        QAction* copyAction = menu.addAction(
            QString::fromUtf8("\u590d\u5236"));
        connect(copyAction, &QAction::triggered, this, [this, path = info.path]() {
            clipPaths_ = QStringList{ path };
            clipIsCut_ = false;
        });
        QAction* cutAction = menu.addAction(
            QString::fromUtf8("\u526a\u5207"));
        connect(cutAction, &QAction::triggered, this, [this, path = info.path]() {
            clipPaths_ = QStringList{ path };
            clipIsCut_ = true;
        });
        if (!clipPaths_.isEmpty()) {
            QAction* pasteAction = menu.addAction(
                clipIsCut_ ? QString::fromUtf8("\u7c98\u8d34 (\u526a\u5207)")
                           : QString::fromUtf8("\u7c98\u8d34"));
            connect(pasteAction, &QAction::triggered, this,
                [this, path = info.path]() {
                    emit pasteRequested(clipPaths_, path, clipIsCut_);
                });
        }
        menu.addSeparator();
        if (!info.isInherited) {
            QAction* newFolderAction = menu.addAction(
                QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939..."));
            connect(newFolderAction, &QAction::triggered, this,
                [this, path = info.path]() {
                    createNewFolder(path);
                });
            QAction* batchAction = menu.addAction(
                QString::fromUtf8("\u6279\u91cf\u8f6c\u6362 JAR\u2192\u6307\u9488"));
            connect(batchAction, &QAction::triggered, this, [this, path = info.path]() {
                emit batchConvertJarsRequested(path);
            });
            QAction* policyAction = menu.addAction(
                QString::fromUtf8("\u4fee\u6539\u6587\u4ef6\u5939\u540c\u6b65\u7b56\u7565..."));
            connect(policyAction, &QAction::triggered, this, [this, info]() {
                emit folderPolicyEditRequested(info);
            });
        }
        menu.addSeparator();
        QAction* delAction = menu.addAction(
            QString::fromUtf8("\u5220\u9664"));
        connect(delAction, &QAction::triggered, this, [this, info]() {
            emit deleteRequested(info);
        });
        menu.exec(event->globalPos());
        return;
    }

    QMenu menu(this);
    const bool textEditable = isTextEditable(item->text(0));
    if (textEditable) {
        QAction* editAction = menu.addAction(
            QString::fromUtf8("\u7f16\u8f91\u5185\u5bb9..."));
        connect(editAction, &QAction::triggered, this, [this, info]() {
            emit contentEditRequested(info);
        });
    }
    if (info.type == RepoObjectType::ConfigFile) {
        QAction* policyAction = menu.addAction(
            QString::fromUtf8("\u4fee\u6539\u6587\u4ef6\u540c\u6b65\u7b56\u7565..."));
        connect(policyAction, &QAction::triggered, this, [this, info]() {
            emit filePolicyEditRequested(info);
        });
    }
    if (!info.isInherited && info.marker != QLatin1String("override")) {
        QAction* convertAction = menu.addAction(
            QString::fromUtf8("\u8f6c\u6307\u9488\u5316"));
        connect(convertAction, &QAction::triggered, this, [this, info]() {
            emit convertToPointerRequested(info);
        });
    }
    if (info.isInherited && !textEditable) {
        QAction* importAction = menu.addAction(
            QString::fromUtf8("\u8986\u76d6\u5bfc\u5165... (\u9009\u62e9\u672c\u5730\u6587\u4ef6)"));
        connect(importAction, &QAction::triggered, this, [this, info]() {
            emit importOverwriteRequested(info);
        });
    }
    if (info.isInherited || !info.marker.isEmpty()) {
        QAction* restoreAction = menu.addAction(
            QString::fromUtf8("\u8fd8\u539f\u7236\u7248\u672c"));
        connect(restoreAction, &QAction::triggered, this, [this, info]() {
            emit restoreInheritedRequested(info);
        });
    }
    menu.addSeparator();
    QAction* copyAction = menu.addAction(
        QString::fromUtf8("\u590d\u5236"));
    connect(copyAction, &QAction::triggered, this, [this, path = info.path]() {
        clipPaths_ = QStringList{ path };
        clipIsCut_ = false;
    });
    QAction* cutAction = menu.addAction(
        QString::fromUtf8("\u526a\u5207"));
    connect(cutAction, &QAction::triggered, this, [this, path = info.path]() {
        clipPaths_ = QStringList{ path };
        clipIsCut_ = true;
    });
    if (!clipPaths_.isEmpty()) {
        QAction* pasteAction = menu.addAction(
            clipIsCut_ ? QString::fromUtf8("\u7c98\u8d34 (\u526a\u5207)")
                       : QString::fromUtf8("\u7c98\u8d34"));
        connect(pasteAction, &QAction::triggered, this, [this, info]() {
            const QString targetRel = clipTarget(info);
            emit pasteRequested(clipPaths_, targetRel, clipIsCut_);
        });
    }
    menu.addSeparator();
    QAction* delAction = menu.addAction(
        QString::fromUtf8("\u5220\u9664"));
    connect(delAction, &QAction::triggered, this, [this, info]() {
        emit deleteRequested(info);
    });
    menu.exec(event->globalPos());
}

void RepoTreePanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete
        || event->key() == Qt::Key_Backspace) {
        const auto sel = selectedObjects();
        if (sel.size() > 1) {
            if (confirmBatchDelete(sel.size())) {
                emit batchDeleteRequested(sel);
            }
            event->accept();
            return;
        }
        if (!sel.isEmpty()) {
            const RepoObjectInfo info = sel.first();
            if (info.type != RepoObjectType::Root
                && info.type != RepoObjectType::Pointer
                && !info.path.isEmpty()) {
                emit deleteRequested(info);
                event->accept();
                return;
            }
        }
    }
    if (event->matches(QKeySequence::Copy)) {
        const auto sel = selectedObjects();
        if (!sel.isEmpty()) {
            QStringList paths;
            for (const auto& info : sel) {
                paths << info.path;
            }
            clipPaths_ = paths;
            clipIsCut_ = false;
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Cut)) {
        const auto sel = selectedObjects();
        if (!sel.isEmpty()) {
            QStringList paths;
            for (const auto& info : sel) {
                paths << info.path;
            }
            clipPaths_ = paths;
            clipIsCut_ = true;
        }
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        if (!clipPaths_.isEmpty()) {
            const RepoObjectInfo cur = currentSelection();
            const QString targetRel = clipTarget(cur);
            emit pasteRequested(clipPaths_, targetRel, clipIsCut_);
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool RepoTreePanel::confirmBatchDelete(int count)
{
    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("\u5220\u9664\u786e\u8ba4"));
    box.setText(QString::fromUtf8("\u786e\u5b9a\u5220\u9664\u9009\u4e2d\u7684 %1 \u4e2a\u9879\u76ee\uff1f\u5220\u9664\u53ef\u901a\u8fc7\u64a4\u9500\u6062\u590d\u3002")
        .arg(count));
    box.setIcon(QMessageBox::Warning);
    box.addButton(QString::fromUtf8("\u5220\u9664"), QMessageBox::AcceptRole);
    QPushButton* cancel = box.addButton(QMessageBox::Cancel);
    box.exec();
    return box.clickedButton() != cancel;
}

QString RepoTreePanel::clipTarget(const RepoObjectInfo& cur) const
{
    if (cur.type == RepoObjectType::Root || cur.path.isEmpty()) {
        return QString();
    }
    if (cur.type == RepoObjectType::Folder) {
        return cur.path;
    }
    return QFileInfo(cur.path).path().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

bool RepoTreePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == tree_) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                dragStartPos_ = me->position().toPoint();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton)
                && (me->position().toPoint() - dragStartPos_).manhattanLength()
                    >= QApplication::startDragDistance()) {
                const auto sel = selectedObjects();
                QStringList rels;
                for (const auto& info : sel) {
                    if (info.type == RepoObjectType::Pointer) continue;
                    if (info.path.isEmpty()) continue;
                    rels << info.path;
                }
                if (!rels.isEmpty()) {
                    auto* mime = new QMimeData;
                    mime->setData(
                        QStringLiteral("application/x-nsum-repo-items"),
                        rels.join(QLatin1Char('\n')).toUtf8());
                    auto* drag = new QDrag(tree_);
                    drag->setMimeData(mime);
                    drag->exec(Qt::MoveAction | Qt::CopyAction);
                }
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RepoTreePanel::createNewFolder(const QString& parentRel)
{
    bool ok = false;
    const QString name = QInputDialog::getText(this,
        QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939"),
        QString::fromUtf8("\u6587\u4ef6\u5939\u540d\u79f0:"),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const QString trimmed = name.trimmed();
    static const QString illegal = QStringLiteral("\\/:*?\"<>|");
    for (const QChar& c : trimmed) {
        if (illegal.contains(c)) {
            QMessageBox::warning(this,
                QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939"),
                QString::fromUtf8("\u540d\u79f0\u542b\u6709\u975e\u6cd5\u5b57\u7b26: %1").arg(c));
            return;
        }
    }

    QString absParent = rootPath_;
    if (!parentRel.isEmpty()) {
        absParent = rootPath_ + QLatin1Char('/') + parentRel;
    }

    QDir parentDir(absParent);
    if (!parentDir.exists()) return;
    if (parentDir.exists(trimmed)) {
        QMessageBox::warning(this,
            QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939"),
            QString::fromUtf8("\u540c\u540d\u6587\u4ef6\u5939\u5df2\u5b58\u5728: %1").arg(trimmed));
        return;
    }

    if (!parentDir.mkpath(trimmed)) {
        QMessageBox::warning(this,
            QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939"),
            QString::fromUtf8("\u521b\u5efa\u5931\u8d25\u3002"));
        return;
    }

    refresh();
}

void RepoTreePanel::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        #repoTreeStatus {
            color: #8a9099;
            font-size: 11px;
        }
    )"));
}

} // namespace HiBerGUI

#include "repo_tree_panel.moc"
