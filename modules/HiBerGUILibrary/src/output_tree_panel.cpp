#include "output_tree_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QBrush>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QFileInfo>
#include <QKeySequence>

namespace HiBerGUI {

OutputTreePanel::OutputTreePanel(QWidget* parent)
    : QWidget(parent)
{
    formatCombo_ = new QComboBox(this);
    formatCombo_->addItem(QString::fromUtf8("HMCL \u5de5\u4f5c\u533a"), "hmcl");
    formatCombo_->addItem(QString::fromUtf8("MCBBS"), "mcbbs");
    formatCombo_->addItem(QString::fromUtf8("Modrinth"), "modrinth");

    refreshButton_ = new QPushButton(QString::fromUtf8("\u5237\u65b0"), this);
    refreshButton_->setFixedWidth(56);

    auto* formatRow = new QHBoxLayout;
    formatRow->setContentsMargins(4, 2, 4, 2);
    formatRow->addWidget(formatCombo_, 1);
    formatRow->addWidget(refreshButton_);

    tree_ = new QTreeWidget(this);
    tree_->setHeaderLabels({ QString::fromUtf8("\u8def\u5f84"),
        QString::fromUtf8("\u7c7b\u578b") });
    tree_->setColumnWidth(0, 230);
    tree_->setAlternatingRowColors(true);
    tree_->setAnimated(true);
    tree_->header()->setStretchLastSection(false);
    tree_->viewport()->setAcceptDrops(true);
    tree_->setAcceptDrops(true);
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->installEventFilter(this);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("outputTreeStatus"));

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(2, 2, 2, 2);
    lay->setSpacing(2);
    lay->addLayout(formatRow);
    lay->addWidget(tree_, 1);
    lay->addWidget(statusLabel_);

    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) {
            emit formatChanged(formatCombo_->currentData().toString());
        });
    connect(refreshButton_, &QPushButton::clicked, this,
        &OutputTreePanel::refreshRequested);

    applyStyle();
}

void OutputTreePanel::loadEntries(const nlohmann::json& entries)
{
    tree_->clear();
    if (!entries.is_array()) {
        setStatusText(QString::fromUtf8("\u65e0\u6cd5\u83b7\u53d6\u9884\u89c8\u7ed3\u6784\u3002"));
        return;
    }

    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    auto umdColor = [darkMode](const QString& umd) -> QColor {
        if (umd == QLatin1String("U")) {
            return darkMode ? QColor(QStringLiteral("#81c784"))
                            : QColor(QStringLiteral("#4caf50"));
        }
        if (umd == QLatin1String("M")) {
            return darkMode ? QColor(QStringLiteral("#ffd54f"))
                            : QColor(QStringLiteral("#f9a825"));
        }
        if (umd == QLatin1String("D")) {
            return darkMode ? QColor(QStringLiteral("#ef5350"))
                            : QColor(QStringLiteral("#e53935"));
        }
        return QColor();
    };

    int fileCount = 0, uCount = 0, mCount = 0, dCount = 0;

    auto addPath = [&](const QString& entry, bool isDir, const QString& umd) {
        const QStringList parts = entry.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QTreeWidgetItem* parent = nullptr;
        QString running;
        for (int i = 0; i < parts.size(); ++i) {
            const bool isLast = (i == parts.size() - 1);
            const QString name = parts[i];
            if (!running.isEmpty()) running += QLatin1Char('/');
            running += name;
            QTreeWidgetItem* match = nullptr;
            const int count = parent ? parent->childCount()
                                     : tree_->topLevelItemCount();
            for (int r = 0; r < count; ++r) {
                QTreeWidgetItem* item = parent
                    ? parent->child(r) : tree_->topLevelItem(r);
                if (item->text(0) == name) {
                    match = item;
                    break;
                }
            }
            if (!match) {
                match = parent ? new QTreeWidgetItem(parent)
                               : new QTreeWidgetItem(tree_);
                match->setText(0, name);
                match->setData(0, Qt::UserRole, running);
                match->setData(0, Qt::UserRole + 2, isLast ? isDir : true);
            }
            match->setText(1, (isLast && !isDir)
                ? QString::fromUtf8("\u6587\u4ef6")
                : QString::fromUtf8("\u76ee\u5f55"));
            if (isLast && !isDir && !umd.isEmpty()) {
                match->setText(0, umd + QLatin1Char(' ') + name);
                match->setData(0, Qt::UserRole + 1, umd);
                const QColor c = umdColor(umd);
                if (c.isValid()) {
                    match->setForeground(0, QBrush(c));
                    match->setForeground(1, QBrush(c));
                }
            }
            parent = match;
        }
    };

    for (const auto& e : entries) {
        if (!e.is_object()) continue;
        const std::string path = e.value("path", std::string());
        if (path.empty()) continue;
        const bool isDir = e.value("dir", false);
        const std::string umd = e.value("umd", std::string());
        addPath(QString::fromStdString(path), isDir, QString::fromStdString(umd));
        if (!isDir) {
            ++fileCount;
            if (umd == "U") ++uCount;
            else if (umd == "M") ++mCount;
            else if (umd == "D") ++dCount;
        }
    }

    tree_->expandToDepth(1);

    QString stat = QString::fromUtf8("\u5305\u5185\u7ed3\u6784\u9884\u89c8\uff0c\u5171 %1 \u4e2a\u6587\u4ef6\u3002")
        .arg(fileCount);
    if (uCount || mCount || dCount) {
        stat += QString::fromUtf8(" U %1 / M %2 / D %3")
            .arg(uCount).arg(mCount).arg(dCount);
    }
    setStatusText(stat);
}

void OutputTreePanel::setStatusText(const QString& text)
{
    statusLabel_->setText(text);
}

void OutputTreePanel::setFormat(const QString& format)
{
    const int idx = formatCombo_->findData(format);
    if (idx >= 0 && idx != formatCombo_->currentIndex()) {
        QSignalBlocker blocker(formatCombo_);
        formatCombo_->setCurrentIndex(idx);
    }
}

QString OutputTreePanel::format() const
{
    return formatCombo_->currentData().toString();
}

RepoObjectInfo OutputTreePanel::currentSelection() const
{
    return infoFromItem(tree_->currentItem());
}

QList<RepoObjectInfo> OutputTreePanel::selectedObjects() const
{
    QList<RepoObjectInfo> result;
    const auto items = tree_->selectedItems();
    for (QTreeWidgetItem* item : items) {
        if (!item) continue;
        const RepoObjectInfo info = infoFromItem(item);
        if (info.type == RepoObjectType::Root || info.path.isEmpty()) continue;
        result << info;
    }
    return result;
}

RepoObjectInfo OutputTreePanel::infoFromItem(QTreeWidgetItem* item) const
{
    RepoObjectInfo info;
    if (!item) return info;
    const QString rel = item->data(0, Qt::UserRole).toString();
    if (rel.isEmpty()) {
        info.type = RepoObjectType::Root;
        return info;
    }
    info.path = rel;
    info.displayName = item->text(0);
    const bool isDir = item->data(0, Qt::UserRole + 2).toBool();
    info.type = isDir ? RepoObjectType::Folder : RepoObjectType::PlainFile;
    info.marker = item->data(0, Qt::UserRole + 1).toString();
    return info;
}

void OutputTreePanel::contextMenuEvent(QContextMenuEvent* event)
{
    QTreeWidgetItem* item = tree_->itemAt(event->pos());
    const RepoObjectInfo clicked = infoFromItem(item);

    const auto sel = selectedObjects();
    if (sel.size() > 1) {
        QMenu menu(this);
        QAction* copyAction = menu.addAction(
            QString::fromUtf8("\u590d\u5236 (%1)").arg(sel.size()));
        connect(copyAction, &QAction::triggered, this, [this, sel]() {
            QStringList paths;
            for (const auto& info : sel) {
                paths << info.path;
            }
            clipPaths_ = paths;
            clipIsCut_ = false;
        });
        QAction* cutAction = menu.addAction(
            QString::fromUtf8("\u526a\u5207 (%1)").arg(sel.size()));
        connect(cutAction, &QAction::triggered, this, [this, sel]() {
            QStringList paths;
            for (const auto& info : sel) {
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
                if (info.marker == QLatin1String("D")) continue;
                deletable << info;
            }
            if (deletable.isEmpty()) return;
            if (confirmBatchDelete(deletable.size())) {
                emit batchDeleteRequested(deletable);
            }
        });
        menu.exec(event->globalPos());
        return;
    }

    if (clicked.type == RepoObjectType::Root) {
        QMenu menu(this);
        QAction* newFolderAction = menu.addAction(
            QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939..."));
        connect(newFolderAction, &QAction::triggered, this, [this]() {
            emit newFolderRequested(QString());
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

    if (clicked.marker == QLatin1String("D")) {
        QMenu menu(this);
        QAction* hint = menu.addAction(
            QString::fromUtf8("\u8be5\u6587\u4ef6\u5df2\u4ece\u6253\u5305\u4e2d\u5220\u9664\u3002"));
        hint->setEnabled(false);
        menu.exec(event->globalPos());
        return;
    }

    QMenu menu(this);
    if (clicked.type == RepoObjectType::Folder) {
        QAction* newFolderAction = menu.addAction(
            QString::fromUtf8("\u65b0\u5efa\u6587\u4ef6\u5939..."));
        connect(newFolderAction, &QAction::triggered, this,
            [this, path = clicked.path]() {
                emit newFolderRequested(path);
            });
        menu.addSeparator();
    }
    QAction* copyAction = menu.addAction(QString::fromUtf8("\u590d\u5236"));
    connect(copyAction, &QAction::triggered, this,
        [this, path = clicked.path]() {
            clipPaths_ = QStringList{ path };
            clipIsCut_ = false;
        });
    QAction* cutAction = menu.addAction(QString::fromUtf8("\u526a\u5207"));
    connect(cutAction, &QAction::triggered, this,
        [this, path = clicked.path]() {
            clipPaths_ = QStringList{ path };
            clipIsCut_ = true;
        });
    if (!clipPaths_.isEmpty()) {
        QAction* pasteAction = menu.addAction(
            clipIsCut_ ? QString::fromUtf8("\u7c98\u8d34 (\u526a\u5207)")
                       : QString::fromUtf8("\u7c98\u8d34"));
        connect(pasteAction, &QAction::triggered, this, [this, clicked]() {
            emit pasteRequested(clipPaths_, clipTarget(clicked), clipIsCut_);
        });
    }
    menu.addSeparator();
    QAction* delAction = menu.addAction(QString::fromUtf8("\u5220\u9664"));
    connect(delAction, &QAction::triggered, this, [this, clicked]() {
        emit deleteRequested(clicked);
    });
    menu.exec(event->globalPos());
}

void OutputTreePanel::keyPressEvent(QKeyEvent* event)
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
            if (info.marker != QLatin1String("D") && !info.path.isEmpty()) {
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
            emit pasteRequested(clipPaths_, clipTarget(currentSelection()),
                clipIsCut_);
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

QString OutputTreePanel::clipTarget(const RepoObjectInfo& cur) const
{
    if (cur.type == RepoObjectType::Root || cur.path.isEmpty()) {
        return QString();
    }
    if (cur.type == RepoObjectType::Folder) {
        return cur.path;
    }
    return QFileInfo(cur.path).path().replace(QLatin1Char('\\'), QLatin1Char('/'));
}

bool OutputTreePanel::confirmBatchDelete(int count)
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

bool OutputTreePanel::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == tree_ && event->type() == QEvent::DragEnter) {
        auto* de = static_cast<QDragEnterEvent*>(event);
        if (de->mimeData()->hasUrls()) {
            de->acceptProposedAction();
        }
        return true;
    }
    if (obj == tree_ && event->type() == QEvent::Drop) {
        auto* drop = static_cast<QDropEvent*>(event);
        const QList<QUrl> urls = drop->mimeData()->urls();
        QStringList paths;
        for (const QUrl& u : urls) {
            if (u.isLocalFile()) {
                paths << u.toLocalFile();
            }
        }
        if (!paths.isEmpty()) {
            const QString relDir = relPathAt(drop->position().toPoint());
            drop->acceptProposedAction();
            emit filesDropped(paths, relDir);
        }
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

QString OutputTreePanel::relPathAt(const QPoint& pos) const
{
    QTreeWidgetItem* item = tree_->itemAt(pos);
    if (!item) return QString();

    const bool isFileItem = (item->text(1) == QString::fromUtf8("\u6587\u4ef6"));
    QStringList parts;
    bool first = true;
    while (item) {
        if (!first || !isFileItem) {
            QString name = item->text(0);
            if (name.size() >= 2 && name.at(1) == QLatin1Char(' ')
                && (name.at(0) == QLatin1Char('U')
                    || name.at(0) == QLatin1Char('M')
                    || name.at(0) == QLatin1Char('D'))) {
                name.remove(0, 2);
            }
            parts.prepend(name);
        }
        first = false;
        item = item->parent();
    }
    QString rel = parts.join(QLatin1Char('/'));
    if (rel == QStringLiteral(".")) {
        rel.clear();
    }
    return rel;
}

void OutputTreePanel::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        #outputTreeStatus {
            color: #8a9099;
            font-size: 11px;
        }
        QComboBox {
            background-color: #2f343c;
            color: #e8eaed;
            border: 1px solid #454b54;
            border-radius: 4px;
            padding: 3px 6px;
        }
        QPushButton {
            background-color: #3a414b;
            color: #e8eaed;
            border: 1px solid #454b54;
            border-radius: 4px;
            padding: 3px 8px;
        }
        QPushButton:hover {
            background-color: #454d59;
        }
    )"));
}

} // namespace HiBerGUI

#include "output_tree_panel.moc"
