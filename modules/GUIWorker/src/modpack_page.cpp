#include "modpack_page.h"
#include "animated_progress.h"

#include <QVBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QFont>
#include <QEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace GUIWorker {

ModpackPage::ModpackPage(QWidget* parent)
    : QWidget(parent)
    , selectedIndex_(-1)
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("\u6574\u5408\u5305\u5206\u652f"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(4);

    auto* subLabel = new QLabel(
        QStringLiteral("\u70b9\u51fb\u4e0b\u65b9\u5206\u652f\u5361\u7247\u8fdb\u884c\u9009\u62e9\uff0c\u7136\u540e\u70b9\u51fb\u201c\u4e0b\u4e00\u6b65\u201d\u786e\u8ba4\u3002"), this);
    subLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    layout->addWidget(subLabel);

    layout->addSpacing(4);

    progress_ = new AnimatedProgress(this);
    progress_->setVisible(false);
    layout->addWidget(progress_);

    layout->addSpacing(6);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll_ = scroll;
    cardContainer_ = new QFrame(scroll);
    cardContainer_->setObjectName("modpackCardContainer");
    cardContainer_->setStyleSheet(
        "#modpackCardContainer { background: transparent; }");
    auto* cardLayout = new QVBoxLayout(cardContainer_);
    cardLayout->setContentsMargins(4, 4, 4, 4);
    cardLayout->setSpacing(10);
    cardLayout->addStretch();
    scroll->setWidget(cardContainer_);
    layout->addWidget(scroll, 1);

    layout->addSpacing(8);

    statusLabel_ = new QLabel(QStringLiteral(""), this);
    statusLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode_ ? QStringLiteral("#7a7f88") : QStringLiteral("#888")));
    layout->addWidget(statusLabel_);

    applyTheme();
}

ModpackPage::~ModpackPage() = default;

bool ModpackPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card) {
            int idx = cards_.indexOf(card);
            if (idx >= 0 && idx < static_cast<int>(branches_.size())) {
                selectedIndex_ = idx;
                updateSelection();
                emit modpackSelected(QString::fromStdString(branches_[idx].name));
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void ModpackPage::loadModpacks(const QString& repoPath)
{
    repoPath_ = repoPath;
    branches_.clear();
    selectedIndex_ = -1;
    for (auto* card : cards_) {
        card->deleteLater();
    }
    cards_.clear();

    statusLabel_->setText(QStringLiteral("\u6b63\u5728\u89e3\u6790\u6574\u5408\u5305\u914d\u7f6e..."));
    progress_->setVisible(true);
    progress_->setIndeterminate(true);
    progress_->setText(QStringLiteral("\u6b63\u5728\u89e3\u6790 workspace.json..."));
    progress_->startAnimation();

    QString wsConfigPath = repoPath_ + QStringLiteral("/workspace.json");
    QFile wsFile(wsConfigPath);
    if (!wsFile.open(QIODevice::ReadOnly)) {
        statusLabel_->setText(QStringLiteral("\u672a\u627e\u5230 workspace.json"));
        progress_->stopAnimation();
        progress_->setVisible(false);
        emit modpacksLoaded(false);
        return;
    }

    QByteArray data = wsFile.readAll();
    wsFile.close();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        statusLabel_->setText(
            QStringLiteral("workspace.json \u89e3\u6790\u5931\u8d25: %1").arg(parseError.errorString()));
        progress_->stopAnimation();
        progress_->setVisible(false);
        emit modpacksLoaded(false);
        return;
    }

    QJsonObject root = doc.object();
    QJsonArray branchesArr = root.value(QStringLiteral("branches")).toArray();

    for (const auto& val : branchesArr) {
        QJsonObject obj = val.toObject();
        ModpackBranchInfo info;
        info.name      = obj.value(QStringLiteral("name")).toString().toStdString();
        info.parent    = obj.value(QStringLiteral("parent")).toString().toStdString();
        info.gameVersion = obj.value(QStringLiteral("game_version")).toString().toStdString();
        if (info.gameVersion.empty()) {
            QString legacy = obj.value(QStringLiteral("gameVersion")).toString();
            if (!legacy.isEmpty())
                info.gameVersion = legacy.toStdString();
        }
        if (info.gameVersion.empty()) {
            info.gameVersion = root.value(QStringLiteral("minecraftVersion")).toString().toStdString();
        }
        info.modloader = obj.value(QStringLiteral("modloader")).toString().toStdString();
        if (info.modloader.empty()) {
            info.modloader = root.value(QStringLiteral("modloader")).toString().toStdString();
        }
        info.modloaderVersion = obj.value(QStringLiteral("modloader_version")).toString().toStdString();
        if (info.modloaderVersion.empty()) {
            QString legacy = obj.value(QStringLiteral("modloaderVersion")).toString();
            info.modloaderVersion = legacy.toStdString();
        }
        info.description = obj.value(QStringLiteral("description")).toString().toStdString();
        info.hidden = obj.value(QStringLiteral("hidden")).toBool(false);
        if (info.hidden) continue;
        branches_.push_back(info);
    }

    if (branches_.empty()) {
        ModpackBranchInfo defaultInfo;
        defaultInfo.name        = "default";
        defaultInfo.gameVersion = root.value(QStringLiteral("minecraftVersion")).toString(
            QStringLiteral("\u672a\u77e5")).toStdString();
        defaultInfo.modloader   = root.value(QStringLiteral("modloader")).toString(
            QStringLiteral("\u672a\u77e5")).toStdString();
        defaultInfo.modloaderVersion = root.value(QStringLiteral("modloader_version")).toString().toStdString();
        branches_.push_back(defaultInfo);
    }

    progress_->stopAnimation();
    progress_->setVisible(false);

    populateCards();
    statusLabel_->setText(
        QStringLiteral("\u5171 %1 \u4e2a\u6574\u5408\u5305\u5206\u652f").arg(static_cast<int>(branches_.size())));
    emit modpacksLoaded(true);
}

void ModpackPage::selectModpack(const QString& name)
{
    for (size_t i = 0; i < branches_.size(); ++i) {
        if (name == QString::fromStdString(branches_[i].name)) {
            selectedIndex_ = static_cast<int>(i);
            updateSelection();
            emit modpackSelected(name);
            return;
        }
    }
}

QString ModpackPage::selectedModpack() const
{
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(branches_.size())) {
        return QString::fromStdString(branches_[selectedIndex_].name);
    }
    return QString();
}

bool ModpackPage::hasSelection() const
{
    return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(branches_.size());
}

QString ModpackPage::summary(const QString& branchName) const
{
    std::string name = branchName.toStdString();
    const ModpackBranchInfo* info = findBranch(name);
    if (!info) return QString();

    QStringList lines;
    if (!info->gameVersion.empty())
        lines << QStringLiteral("\u6e38\u620f\u7248\u672c: %1").arg(QString::fromStdString(info->gameVersion));
    if (!info->modloader.empty())
        lines << QStringLiteral("\u52a0\u8f7d\u5668: %1 %2")
            .arg(QString::fromStdString(info->modloader),
                 QString::fromStdString(info->modloaderVersion));
    if (!info->description.empty())
        lines << QStringLiteral("\u8bf4\u660e: %1").arg(QString::fromStdString(info->description));
    return lines.join(QLatin1Char('\n'));
}

QFrame* ModpackPage::createCard(const ModpackBranchInfo& info)
{
    auto* card = new QFrame(cardContainer_);
    card->setObjectName(QStringLiteral("modpackCard"));
    card->setCursor(Qt::PointingHandCursor);
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(cardStyle(false));
    card->installEventFilter(this);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 12, 16, 12);

    auto* textLabel = new QLabel(card);
    textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    textLabel->setTextFormat(Qt::RichText);
    textLabel->setWordWrap(true);

    QStringList metaParts;
    if (!info.gameVersion.empty()) metaParts << QString::fromStdString(info.gameVersion);
    if (!info.modloader.empty()) {
        QString l = QString::fromStdString(info.modloader);
        if (!info.modloaderVersion.empty()) l += " " + QString::fromStdString(info.modloaderVersion);
        metaParts << l;
    }

    QString html = QStringLiteral("<b>%1</b>").arg(QString::fromStdString(info.name).toHtmlEscaped());
    if (!metaParts.isEmpty()) {
        html += QStringLiteral("<span style='font-size:12px; color:%1;'>  %2</span>")
            .arg(darkMode_ ? QStringLiteral("#9aa0a8") : QStringLiteral("#888"),
                 metaParts.join(QLatin1String("  \u00b7  ")).toHtmlEscaped());
    }
    if (!info.description.empty()) {
        html += QStringLiteral("<br><span style='font-size:13px; color:%1;'>%2</span>")
            .arg(darkMode_ ? QStringLiteral("#d8d8de") : QStringLiteral("#333"),
                 QString::fromStdString(info.description).toHtmlEscaped());
    }
    textLabel->setText(html);
    cardLayout->addWidget(textLabel);

    return card;
}

void ModpackPage::populateCards()
{
    for (auto* card : cards_) card->deleteLater();
    cards_.clear();

    auto* layout = qobject_cast<QVBoxLayout*>(cardContainer_->layout());
    if (!layout) return;
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }
    layout->addStretch();

    for (size_t i = 0; i < branches_.size(); ++i) {
        auto* card = createCard(branches_[i]);
        layout->insertWidget(layout->count() - 1, card);
        cards_.push_back(card);
    }
}

int ModpackPage::contentHeight(int widthHint) const
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
    if (!layout) return 0;

    int cardW = widthHint - 40 - 8 - 32;
    if (cardW < 200) cardW = 200;

    int cardsH = 0;
    for (auto* card : cards_) {
        QLayout* cl = card->layout();
        int h = cl ? cl->heightForWidth(cardW) : -1;
        if (h < 0) h = card->sizeHint().height();
        cardsH += h;
    }
    int n = cards_.size();
    cardsH += 10 * qMax(0, n - 1);
    cardsH += 8;

    int oneCard = cards_.isEmpty() ? 64 : qMax(64, cards_.first()->layout()
        ? cards_.first()->layout()->heightForWidth(cardW) : cards_.first()->sizeHint().height());
    cardsH = qMax(cardsH, oneCard);

    int spacing = layout->spacing();
    if (spacing < 0) spacing = 6;

    int h = layout->contentsMargins().top() + layout->contentsMargins().bottom();
    int count = layout->count();
    for (int i = 0; i < count; ++i) {
        QLayoutItem* item = layout->itemAt(i);
        if (QWidget* w = item->widget()) {
            if (w == scroll_) {
                h += cardsH;
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

void ModpackPage::updateSelection()
{
    for (int i = 0; i < cards_.size(); ++i) {
        if (!cards_[i]) continue;
        bool selected = (i == selectedIndex_);
        cards_[i]->setStyleSheet(cardStyle(selected));
    }
}

QString ModpackPage::cardStyle(bool selected) const
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
        return QStringLiteral("QFrame#modpackCard { background: rgba(%1,%2,%3,%4); border: 2px solid %5; border-radius: 10px; }")
            .arg(selectedBg.red()).arg(selectedBg.green()).arg(selectedBg.blue()).arg(selectedBg.alpha())
            .arg(selectedBorder.name());
    }
    QString hoverBorder = highlight.name();
    return QStringLiteral("QFrame#modpackCard { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QFrame#modpackCard:hover { border: 1px solid %3; }")
        .arg(cardBg.name(), cardBorder.name(), hoverBorder);
}

void ModpackPage::applyTheme()
{
    for (int i = 0; i < cards_.size(); ++i) {
        if (!cards_[i]) continue;
        cards_[i]->setStyleSheet(cardStyle(i == selectedIndex_));
    }
}

const ModpackBranchInfo* ModpackPage::findBranch(
    const std::string& name) const
{
    for (const auto& b : branches_) {
        if (b.name == name) return &b;
    }
    return nullptr;
}

} // namespace GUIWorker

#include "modpack_page.moc"