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
        for (int i = 0; i < parts.size(); ++i) {
            const bool isLast = (i == parts.size() - 1);
            const QString name = parts[i];
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
            }
            match->setText(1, (isLast && !isDir)
                ? QString::fromUtf8("\u6587\u4ef6")
                : QString::fromUtf8("\u76ee\u5f55"));
            if (isLast && !isDir && !umd.isEmpty()) {
                match->setText(0, umd + QLatin1Char(' ') + name);
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
