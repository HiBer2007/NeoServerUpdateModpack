#include "branch_page.h"
#include "animated_progress.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QFont>
#include <QEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QSignalBlocker>

namespace GUIWorker {

BranchPage::BranchPage(QWidget* parent)
    : QWidget(parent)
    , selectedIndex_(-1)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QStringLiteral("\u9009\u62e9 Git \u5206\u652f"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(10);

    repoLabel_ = new QLabel(QStringLiteral("\u4ed3\u5e93: (\u672a\u52a0\u8f7d)"), this);
    repoLabel_->setWordWrap(true);
    layout->addWidget(repoLabel_);

    layout->addSpacing(8);

    progress_ = new AnimatedProgress(this);
    progress_->setVisible(false);
    layout->addWidget(progress_);

    layout->addSpacing(8);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll_ = scroll;
    cardContainer_ = new QFrame(scroll);
    cardContainer_->setObjectName("branchCardContainer");
    cardContainer_->setStyleSheet(
        "#branchCardContainer { background: transparent; }");
    auto* cardLayout = new QVBoxLayout(cardContainer_);
    cardLayout->setContentsMargins(4, 4, 4, 4);
    cardLayout->setSpacing(10);
    cardLayout->addStretch();
    scroll->setWidget(cardContainer_);
    layout->addWidget(scroll, 1);

    statusLabel_ = new QLabel(QStringLiteral(""), this);
    layout->addWidget(statusLabel_);

    applyTheme();
}

BranchPage::~BranchPage() = default;

bool BranchPage::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        QFrame* card = qobject_cast<QFrame*>(obj);
        if (card) {
            int idx = cards_.indexOf(card);
            if (idx >= 0 && idx < static_cast<int>(branches_.size())) {
                selectedIndex_ = idx;
                updateSelection();
                emit branchSelected(QString::fromStdString(branches_[idx].name));
            }
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void BranchPage::showLoading(const QString& status, int percent)
{
    progress_->setVisible(true);
    if (percent >= 0) {
        progress_->setIndeterminate(false);
        progress_->setValue(percent);
    } else {
        progress_->setIndeterminate(true);
    }
    progress_->setText(status);
    progress_->startAnimation();
    statusLabel_->setText(status);
}

void BranchPage::stopLoading()
{
    progress_->stopAnimation();
    progress_->setVisible(false);
}

void BranchPage::loadBranches(const QString& repoPath)
{
    repoPath_ = repoPath;
    repoLabel_->setText(QStringLiteral("\u4ed3\u5e93: %1").arg(repoPath_));

    selectedIndex_ = -1;
    for (auto* card : cards_) {
        card->deleteLater();
    }
    cards_.clear();

    auto* layout = qobject_cast<QVBoxLayout*>(cardContainer_->layout());
    if (layout) {
        while (layout->count() > 0) {
            auto* item = layout->takeAt(0);
            delete item->widget();
            delete item;
        }
        layout->addStretch();
    }

    statusLabel_->setText(QStringLiteral("\u6b63\u5728\u8bfb\u53d6\u5206\u652f\u5217\u8868..."));
    progress_->setVisible(true);
    progress_->setIndeterminate(true);
    progress_->setText(QStringLiteral("\u6b63\u5728\u8bfb\u53d6\u5206\u652f\u5217\u8868..."));
    progress_->startAnimation();

    collectBranches();
    populateUI();

    progress_->stopAnimation();
    progress_->setVisible(false);
    emit branchesLoaded(true);
}

QString BranchPage::selectedBranch() const
{
    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(branches_.size())) {
        return QString::fromStdString(branches_[selectedIndex_].name);
    }
    return QString();
}

bool BranchPage::hasSelection() const
{
    return selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(branches_.size());
}

void BranchPage::selectBranch(const QString& name)
{
    for (size_t i = 0; i < branches_.size(); ++i) {
        if (name == QString::fromStdString(branches_[i].name)) {
            selectedIndex_ = static_cast<int>(i);
            updateSelection();
            emit branchSelected(name);
            return;
        }
    }
}

void BranchPage::collectBranches()
{
    branches_.clear();

    QProcess p;
    p.setWorkingDirectory(repoPath_);
    p.start(QStringLiteral("git"), {
        QStringLiteral("for-each-ref"),
        QStringLiteral("--format=%(refname)"),
        QStringLiteral("refs/remotes/origin"),
        QStringLiteral("refs/heads") });
    if (!p.waitForFinished(15000)) return;

    const QString output = QString::fromUtf8(p.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (const auto& line : lines) {
        QString ref = line.trimmed();
        QString name;
        if (ref.startsWith(QLatin1String("refs/remotes/origin/"))) {
            name = ref.mid(20);
        } else if (ref.startsWith(QLatin1String("refs/heads/"))) {
            name = ref.mid(11);
        } else {
            continue;
        }
        if (name.isEmpty() || name == QLatin1String("HEAD")) continue;

        GitBranchInfo info;
        info.name = name.toStdString();
        readBranchInfo(info);
        branches_.push_back(info);
    }

    QString defaultBranch;
    QProcess head;
    head.setWorkingDirectory(repoPath_);
    head.start(QStringLiteral("git"), { QStringLiteral("symbolic-ref"),
        QStringLiteral("--short"), QStringLiteral("refs/remotes/origin/HEAD") });
    if (head.waitForFinished(8000)) {
        defaultBranch = QString::fromUtf8(head.readAllStandardOutput()).trimmed();
        if (defaultBranch.startsWith(QLatin1String("origin/"))) {
            defaultBranch = defaultBranch.mid(7);
        }
    }
    if (defaultBranch.isEmpty() && !branches_.empty()) {
        for (const auto& b : branches_) {
            if (b.name == "master" || b.name == "main" || b.name == "trunk") {
                defaultBranch = QString::fromStdString(b.name);
                break;
            }
        }
        if (defaultBranch.isEmpty())
            defaultBranch = QString::fromStdString(branches_.front().name);
    }
    for (auto& b : branches_) {
        b.isDefault = (QString::fromStdString(b.name) == defaultBranch);
    }
}

void BranchPage::readBranchInfo(GitBranchInfo& info) const
{
    QProcess p;
    p.setWorkingDirectory(repoPath_);
    p.start(QStringLiteral("git"), {
        QStringLiteral("show"), QString::fromStdString(info.name)
            + QStringLiteral(":workspace.json") });
    if (!p.waitForFinished(8000)) return;

    const QByteArray data = p.readAllStandardOutput();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    QString desc = root.value(QStringLiteral("description")).toString();
    if (desc.isEmpty()) {
        desc = root.value(QStringLiteral("workspace"))
            .toObject().value(QStringLiteral("description")).toString();
    }
    info.description = desc.toStdString();
    info.hidden = root.value(QStringLiteral("hidden")).toBool(false);
}

void BranchPage::populateUI()
{
    auto* layout = qobject_cast<QVBoxLayout*>(cardContainer_->layout());
    if (!layout) return;
    while (layout->count() > 0) {
        auto* item = layout->takeAt(0);
        delete item->widget();
        delete item;
    }
    layout->addStretch();

    if (branches_.empty()) {
        statusLabel_->setText(QStringLiteral("\u8fd9\u4e2a\u4ed3\u5e93\u6ca1\u6709\u6709\u6548\u5206\u652f"));
        return;
    }

    for (size_t i = 0; i < branches_.size(); ++i) {
        const GitBranchInfo& info = branches_[i];
        if (info.hidden) continue;

        auto* card = new QFrame(cardContainer_);
        card->setObjectName(QStringLiteral("branchCard"));
        card->setCursor(Qt::PointingHandCursor);
        card->setFrameShape(QFrame::NoFrame);
        card->setStyleSheet(cardStyle(false));
        card->installEventFilter(this);

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(16, 12, 16, 12);
        cardLayout->setSpacing(6);

        auto* textLabel = new QLabel(card);
        textLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        textLabel->setTextFormat(Qt::RichText);
        textLabel->setWordWrap(true);

        QString nameText = QString::fromStdString(info.name);
        if (info.isDefault) {
            nameText += QStringLiteral("  (\u9ed8\u8ba4)");
        }
        QString descText = QString::fromStdString(info.description);
        QString html = QStringLiteral("<b>%1</b>").arg(nameText.toHtmlEscaped());
        if (!descText.isEmpty()) {
            html += QStringLiteral("<br><span style='font-size:12px; color:%2;'>%3</span>")
                .arg(darkMode_ ? QStringLiteral("#9aa0a8") : QStringLiteral("#666"),
                     descText.toHtmlEscaped());
        }
        textLabel->setText(html);
        textLabel->setStyleSheet(QStringLiteral("color: %1;")
            .arg(darkMode_ ? QStringLiteral("#e8e8e8") : QStringLiteral("#000000")));
        cardLayout->addWidget(textLabel);

        layout->insertWidget(layout->count() - 1, card);
        cards_.push_back(card);
    }

    statusLabel_->setText(
        QStringLiteral("\u5171 %1 \u4e2a Git \u5206\u652f\uff0c\u8bf7\u9009\u62e9\u4e00\u4e2a").arg(
            static_cast<int>(cards_.size())));
}

int BranchPage::contentHeight(int widthHint) const
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

    if (!cards_.isEmpty()) {
        int oneCard = qMax(64, cards_.first()->layout()
            ? cards_.first()->layout()->heightForWidth(cardW) : cards_.first()->sizeHint().height());
        cardsH = qMax(cardsH, oneCard);
    }

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

void BranchPage::updateSelection()
{
    for (int i = 0; i < cards_.size(); ++i) {
        if (!cards_[i]) continue;
        bool selected = (i == selectedIndex_);
        cards_[i]->setStyleSheet(cardStyle(selected));
    }
}

QString BranchPage::cardStyle(bool selected) const
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
        return QStringLiteral("QFrame#branchCard { background: rgba(%1,%2,%3,%4); border: 2px solid %5; border-radius: 10px; }")
            .arg(selectedBg.red()).arg(selectedBg.green()).arg(selectedBg.blue()).arg(selectedBg.alpha())
            .arg(selectedBorder.name());
    }
    QString hoverBorder = highlight.name();
    return QStringLiteral("QFrame#branchCard { background: %1; border: 1px solid %2; border-radius: 10px; }"
        "QFrame#branchCard:hover { border: 1px solid %3; }")
        .arg(cardBg.name(), cardBorder.name(), hoverBorder);
}

void BranchPage::applyTheme()
{
    const QColor windowBg = palette().color(QPalette::Window);
    darkMode_ = windowBg.lightness() < 128;

    const QString dimColor   = darkMode_ ? QStringLiteral("#9da2aa") : QStringLiteral("#555");
    const QString faintColor = darkMode_ ? QStringLiteral("#7a7f88") : QStringLiteral("#888");

    repoLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px; margin-bottom: 8px;").arg(dimColor));
    statusLabel_->setStyleSheet(
        QStringLiteral("color: %1; font-size: 12px;").arg(faintColor));
    for (int i = 0; i < cards_.size(); ++i) {
        if (!cards_[i]) continue;
        cards_[i]->setStyleSheet(cardStyle(i == selectedIndex_));
    }
}

} // namespace GUIWorker

#include "branch_page.moc"
