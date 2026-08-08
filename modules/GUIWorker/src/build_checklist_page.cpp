#include "build_checklist_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFont>
#include <QHeaderView>
#include <QScrollArea>
#include <QColor>
#include <QPalette>
#include <QBrush>
#include <functional>

namespace GUIWorker {

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* content,
                                       QWidget* parent)
    : QWidget(parent)
    , content_(content)
    , expanded_(true)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    headerBtn_ = new QPushButton(title, this);
    headerBtn_->setCheckable(false);
    headerBtn_->setFlat(true);
    headerBtn_->setStyleSheet(QStringLiteral(
        "QPushButton { text-align: left; font-weight: bold; font-size: 13px;"
        "  border: none; padding: 2px 4px; color: palette(text); }"
        "QPushButton:hover { color: palette(highlight); }"));
    layout->addWidget(headerBtn_);
    layout->addWidget(content_);

    connect(headerBtn_, &QPushButton::clicked, this, &CollapsibleSection::toggle);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    if (expanded_ == expanded) return;
    expanded_ = expanded;
    content_->setVisible(expanded);
    headerBtn_->setText(QString(expanded ? QStringLiteral("\u25bc ") : QStringLiteral("\u25b6 "))
        + headerBtn_->text().mid(2));
    emit expandedChanged();
}

bool CollapsibleSection::isExpanded() const
{
    return expanded_;
}

void CollapsibleSection::toggle()
{
    setExpanded(!expanded_);
}

int CollapsibleSection::contentHeight(int widthHint) const
{
    Q_UNUSED(widthHint);
    int h = headerBtn_->sizeHint().height() + 2;
    if (expanded_) {
        h += content_->sizeHint().height();
    }
    return h;
}

BuildChecklistPage::BuildChecklistPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QString::fromUtf8("\u6784\u5efa\u6e05\u5355"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    configSection_ = new CollapsibleSection(
        QString::fromUtf8("\u25bc \u6784\u5efa\u914d\u7f6e"),
        new QWidget(this), this);
    auto* configContent = configSection_->content();
    auto* configLayout = new QVBoxLayout(configContent);
    configLayout->setContentsMargins(4, 2, 4, 2);
    summaryText_ = new QLabel(configContent);
    summaryText_->setWordWrap(true);
    summaryText_->setTextFormat(Qt::RichText);
    summaryText_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
        .arg(darkMode ? QStringLiteral("#d8d8de") : QStringLiteral("#777")));
    configLayout->addWidget(summaryText_);
    layout->addWidget(configSection_);

    extraSection_ = new CollapsibleSection(
        QString::fromUtf8("\u25b6 \u6269\u5c55\u4fe1\u606f"),
        new QWidget(this), this);
    auto* extraContent = extraSection_->content();
    auto* extraLayout = new QVBoxLayout(extraContent);
    extraLayout->setContentsMargins(4, 2, 4, 2);
    extraText_ = new QLabel(extraContent);
    extraText_->setWordWrap(true);
    extraText_->setTextFormat(Qt::RichText);
    extraText_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
        .arg(darkMode ? QStringLiteral("#d8d8de") : QStringLiteral("#777")));
    extraLayout->addWidget(extraText_);
    extraSection_->setExpanded(false);
    layout->addWidget(extraSection_);

    treeSection_ = new CollapsibleSection(
        QString::fromUtf8("\u25b6 \u6587\u4ef6\u6811\u9884\u89c8"),
        new QWidget(this), this);
    auto* treeContent = treeSection_->content();
    auto* treeLayout = new QVBoxLayout(treeContent);
    treeLayout->setContentsMargins(4, 2, 4, 2);

    fileTree_ = new QTreeWidget(treeContent);
    fileTree_->setHeaderLabels({QStringLiteral("\u8def\u5f84"), QStringLiteral("\u7c7b\u578b")});
    fileTree_->header()->setStretchLastSection(true);
    fileTree_->setRootIsDecorated(true);
    fileTree_->setAlternatingRowColors(true);
    fileTree_->setColumnWidth(0, 380);
    treeLayout->addWidget(fileTree_);

    auto* treeBtnRow = new QHBoxLayout();
    expandBtn_ = new QPushButton(QString::fromUtf8("\u5c55\u5f00\u5168\u90e8"), treeContent);
    collapseBtn_ = new QPushButton(QString::fromUtf8("\u6536\u8d77\u5168\u90e8"), treeContent);
    treeBtnRow->addStretch();
    treeBtnRow->addWidget(expandBtn_);
    treeBtnRow->addWidget(collapseBtn_);
    treeLayout->addLayout(treeBtnRow);

    statusLabel_ = new QLabel(treeContent);
    statusLabel_->setStyleSheet(QStringLiteral("color: #888; font-size: 12px;"));
    treeLayout->addWidget(statusLabel_);

    treeSection_->setExpanded(false);
    layout->addWidget(treeSection_);

    layout->addStretch();

    connect(expandBtn_, &QPushButton::clicked, this, &BuildChecklistPage::expandAll);
    connect(collapseBtn_, &QPushButton::clicked, this, &BuildChecklistPage::collapseAll);
    connect(fileTree_, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem*) {
        emit treeExpandedChanged();
    });
    connect(fileTree_, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem*) {
        emit treeExpandedChanged();
    });
    connect(configSection_, &CollapsibleSection::expandedChanged, this, [this]() {
        emit treeExpandedChanged();
    });
    connect(extraSection_, &CollapsibleSection::expandedChanged, this, [this]() {
        emit treeExpandedChanged();
    });
    connect(treeSection_, &CollapsibleSection::expandedChanged, this, [this]() {
        emit treeExpandedChanged();
    });
}

void BuildChecklistPage::setSummary(const QString& key, const QString& value)
{
    QString text = summaryText_->text();
    QString escapedValue = value.toHtmlEscaped();
    if (!text.isEmpty()) text += QStringLiteral("<br>");
    text += QStringLiteral("<b>%1:</b> %2").arg(key.toHtmlEscaped(), escapedValue);
    summaryText_->setText(text);
}

void BuildChecklistPage::clearSummary()
{
    summaryText_->clear();
}

void BuildChecklistPage::setExtraInfo(const QMap<QString, QString>& values)
{
    extraValues_ = values;
    QString text;
    if (values.isEmpty()) {
        text = QString::fromUtf8("\u6ca1\u6709\u9700\u8981\u7684\u6269\u5c55\u4fe1\u606f");
    } else {
        for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
            if (!text.isEmpty()) text += QStringLiteral("<br>");
            text += QStringLiteral("<b>%1:</b> %2")
                .arg(it.key().toHtmlEscaped(), it.value().toHtmlEscaped());
        }
    }
    extraText_->setText(text);
}

void BuildChecklistPage::loadStructure(const nlohmann::json& entries)
{
    fileTree_->clear();
    statusLabel_->clear();

    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    auto umdColor = [darkMode](const QString& umd) -> QColor {
        if (umd == QLatin1String("U")) {
            return darkMode ? QColor(QStringLiteral("#81c784")) : QColor(QStringLiteral("#4caf50"));
        }
        if (umd == QLatin1String("M")) {
            return darkMode ? QColor(QStringLiteral("#ffd54f")) : QColor(QStringLiteral("#f9a825"));
        }
        if (umd == QLatin1String("D")) {
            return darkMode ? QColor(QStringLiteral("#ef5350")) : QColor(QStringLiteral("#e53935"));
        }
        return QColor();
    };

    if (!entries.is_array()) {
        statusLabel_->setText(QString::fromUtf8("\u65e0\u6cd5\u83b7\u53d6\u5305\u7ed3\u6784\u9884\u89c8\u3002"));
        return;
    }

    int fileCount = 0, uCount = 0, mCount = 0, dCount = 0;

    auto addPath = [&](const QString& entry, bool isDir, const QString& umd) {
        QStringList parts = entry.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        QTreeWidgetItem* parent = nullptr;
        for (int i = 0; i < parts.size(); ++i) {
            bool isLast = (i == parts.size() - 1);
            QString name = parts[i];
            QTreeWidgetItem* match = nullptr;
            QTreeWidgetItem* container = parent;
            int count = container ? container->childCount()
                                  : fileTree_->topLevelItemCount();
            for (int r = 0; r < count; ++r) {
                QTreeWidgetItem* item = container
                    ? container->child(r) : fileTree_->topLevelItem(r);
                if (item->text(0) == name) {
                    match = item;
                    break;
                }
            }
            if (!match) {
                match = container
                    ? new QTreeWidgetItem(container)
                    : new QTreeWidgetItem(fileTree_);
                match->setText(0, name);
            }
            match->setText(1, (isLast && !isDir) ? QStringLiteral("\u6587\u4ef6")
                                                 : QStringLiteral("\u76ee\u5f55"));
            if (isLast && !isDir && !umd.isEmpty()) {
                match->setText(0, umd + QLatin1Char(' ') + name);
                QColor c = umdColor(umd);
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
        std::string path = e.value("path", std::string());
        if (path.empty()) continue;
        bool isDir = e.value("dir", false);
        std::string umd = e.value("umd", std::string());
        addPath(QString::fromStdString(path), isDir, QString::fromStdString(umd));
        if (!isDir) {
            ++fileCount;
            if (umd == "U") ++uCount;
            else if (umd == "M") ++mCount;
            else if (umd == "D") ++dCount;
        }
    }

    fileTree_->expandToDepth(1);
    QString stat = QString::fromUtf8("\u5305\u5185\u7ed3\u6784\u9884\u89c8\uff0c\u5171 %1 \u4e2a\u6587\u4ef6\u3002")
        .arg(fileCount);
    if (uCount || mCount || dCount) {
        stat += QString::fromUtf8(" U %1 / M %2 / D %3")
            .arg(uCount).arg(mCount).arg(dCount);
    }
    statusLabel_->setText(stat);
}

void BuildChecklistPage::clearFileTree()
{
    fileTree_->clear();
    statusLabel_->clear();
}

void BuildChecklistPage::expandAll()
{
    fileTree_->expandAll();
}

void BuildChecklistPage::collapseAll()
{
    fileTree_->collapseAll();
}

int BuildChecklistPage::contentHeight(int widthHint) const
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (!layout) return 0;

    int visibleRows = 0;
    std::function<void(const QTreeWidgetItem*)> countVisible = [&](const QTreeWidgetItem* item) {
        if (!item->isExpanded() || item->childCount() == 0) {
            visibleRows += 1;
            return;
        }
        visibleRows += 1;
        for (int i = 0; i < item->childCount(); ++i) {
            countVisible(item->child(i));
        }
    };
    for (int i = 0; i < fileTree_->topLevelItemCount(); ++i) {
        countVisible(fileTree_->topLevelItem(i));
    }

    int rowH = 22;
    if (fileTree_->topLevelItemCount() > 0) {
        int rh = fileTree_->visualItemRect(fileTree_->topLevelItem(0)).height();
        if (rh > 0) rowH = rh;
    }
    int treeH = qMax(visibleRows * rowH, 80);

    Q_UNUSED(widthHint);
    int spacing = layout->spacing();
    if (spacing < 0) spacing = 6;

    int h = layout->contentsMargins().top() + layout->contentsMargins().bottom();
    int count = layout->count();
    for (int i = 0; i < count; ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (QWidget* w = item->widget()) {
            if (auto* section = qobject_cast<CollapsibleSection*>(w)) {
                if (section == treeSection_) {
                    h += section->headerHeight() + 2;
                    if (section->isExpanded()) {
                        h += treeH;
                        h += expandBtn_->sizeHint().height() + 4;
                        h += statusLabel_->sizeHint().height() + 4;
                    }
                } else {
                    h += section->contentHeight(widthHint);
                }
            } else if (w->isVisibleTo(this)) {
                h += w->sizeHint().height();
            }
        } else if (QSpacerItem* sp = item->spacerItem()) {
            h += sp->sizeHint().height();
        }
        if (i < count - 1) h += spacing;
    }
    return h;
}

} // namespace GUIWorker

#include "build_checklist_page.moc"
