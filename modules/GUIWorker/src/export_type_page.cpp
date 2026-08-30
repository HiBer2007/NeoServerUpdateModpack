#include "export_type_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QEvent>
#include <QMouseEvent>
#include <QFont>
#include <QPalette>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace GUIWorker {

ExportTypePage::ExportTypePage(QWidget* parent)
    : QWidget(parent)
    , selectedIndex_(-1)
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    scanExporters();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("\u9009\u62e9\u5bfc\u51fa\u7c7b\u578b"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    auto* subLabel = new QLabel(
        QStringLiteral("\u70b9\u51fb\u4e0b\u65b9\u5361\u7247\u9009\u62e9\u5bfc\u51fa\u683c\u5f0f\u3002"), this);
    subLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    layout->addWidget(subLabel);

    layout->addSpacing(10);

    for (size_t i = 0; i < formats_.size(); ++i) {
        auto* card = createCard(formats_[i], static_cast<int>(i));
        cards_.push_back(card);
        layout->addWidget(card);
    }

    layout->addStretch();
}

bool ExportTypePage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card) {
            auto it = std::find(cards_.begin(), cards_.end(), card);
            if (it != cards_.end()) {
                int idx = static_cast<int>(std::distance(cards_.begin(), it));
                if (idx >= 0 && idx < static_cast<int>(formats_.size())) {
                    selectedIndex_ = idx;
                    updateSelection();
                    emit formatSelected(QString::fromStdString(formats_[idx].id));
                }
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

QString ExportTypePage::selectedFormat() const
{
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(formats_.size())) {
        return QString::fromStdString(formats_[selectedIndex_].id);
    }
    return QString();
}

QString ExportTypePage::selectedFormatName() const
{
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(formats_.size())) {
        return QString::fromStdString(formats_[selectedIndex_].name);
    }
    return QString();
}

void ExportTypePage::selectFormat(const QString& id)
{
    for (size_t i = 0; i < formats_.size(); ++i) {
        if (id == QString::fromStdString(formats_[i].id)) {
            selectedIndex_ = static_cast<int>(i);
            updateSelection();
            emit formatSelected(id);
            return;
        }
    }
}

bool ExportTypePage::hasSelection() const
{
    return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(formats_.size());
}

void ExportTypePage::scanExporters()
{
    formats_.clear();

    QString dir = QApplication::applicationDirPath() + QStringLiteral("/exporters");
    QDir exportersDir(dir);
    if (exportersDir.exists()) {
        const QStringList files = exportersDir.entryList(
            {QStringLiteral("*.meta.json")}, QDir::Files, QDir::Name);
        for (const auto& f : files) {
            QFile file(exportersDir.filePath(f));
            if (!file.open(QIODevice::ReadOnly)) continue;
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            file.close();
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) continue;

            QJsonObject obj = doc.object();
            ExportFormat fmt;
            fmt.id = obj.value(QStringLiteral("format")).toString().toStdString();
            fmt.extension = obj.value(QStringLiteral("extension")).toString().toStdString();
            const QString dispName = fmt.extension.empty()
                ? QString::fromStdString(fmt.id)
                : QString::fromStdString(fmt.id)
                    + QStringLiteral(" (%1)").arg(
                        QString::fromStdString(fmt.extension));
            fmt.name = dispName.toStdString();
            fmt.description = obj.value(QStringLiteral("description")).toString().toStdString();
            // 空扩展名 = 目录型导出 (HMCL 工作区同步), 合法格式
            if (!fmt.id.empty()) {
                formats_.push_back(fmt);
            }
        }
    }

    if (formats_.empty()) {
        formats_ = {
            {"mcbbs", "MCBBS \u6574\u5408\u5305 (.zip)",
             ".zip",
             "MCBBS / PCL / HMCL \u901a\u7528\u6574\u5408\u5305\u683c\u5f0f\u3002"},
            {"modrinth", "Modrinth (.mrpack)",
             ".mrpack",
             "Modrinth Modpack Format\u3002"},
            {"hmcl", "HMCL \u5de5\u4f5c\u76ee\u5f55",
             ".zip",
             "HMCL \u539f\u751f\u683c\u5f0f\u3002"}
        };
    }
}

QFrame* ExportTypePage::createCard(const ExportFormat& fmt, int index)
{
    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("exportTypeCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(cardStyle(false));
    card->installEventFilter(this);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);
    cardLayout->setSpacing(6);

    auto* nameLabel = new QLabel(QString::fromStdString(fmt.name), card);
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(13);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setStyleSheet(QStringLiteral("color: %1;")
        .arg(darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000")));
    cardLayout->addWidget(nameLabel);

    auto* descLabel = new QLabel(QString::fromStdString(fmt.description), card);
    descLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode_ ? QStringLiteral("#9aa0a8") : QStringLiteral("#555")));
    cardLayout->addWidget(descLabel);

    Q_UNUSED(index);
    return card;
}

void ExportTypePage::updateSelection()
{
    for (size_t i = 0; i < cards_.size(); ++i) {
        bool selected = (static_cast<int>(i) == selectedIndex_);
        cards_[i]->setStyleSheet(cardStyle(selected));
    }
}

QString ExportTypePage::cardStyle(bool selected) const
{
    const QColor base = palette().color(QPalette::Base);
    const QColor window = palette().color(QPalette::Window);
    const QColor highlight = palette().color(QPalette::Highlight);

    QColor cardBg = base;
    if (darkMode_ && base.lightness() > window.lightness()) {
        cardBg = base.darker(112);
    }
    QColor cardBorder = darkMode_ ? QColor(QStringLiteral("#55585e"))
                                  : QColor(QStringLiteral("#e0e0e0"));
    QColor selectedBg = highlight;
    selectedBg.setAlpha(40);
    QColor selectedBorder = highlight;

    if (selected) {
        return QStringLiteral("QFrame#exportTypeCard { background: rgba(%1,%2,%3,%4); border: 2px solid %5; border-radius: 10px; }")
            .arg(selectedBg.red()).arg(selectedBg.green()).arg(selectedBg.blue()).arg(selectedBg.alpha())
            .arg(selectedBorder.name());
    }
    QString hoverBorder = highlight.name();
    return QStringLiteral("QFrame#exportTypeCard { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QFrame#exportTypeCard:hover { border: 1px solid %3; }")
        .arg(cardBg.name(), cardBorder.name(), hoverBorder);
}

} // namespace GUIWorker

#include "export_type_page.moc"