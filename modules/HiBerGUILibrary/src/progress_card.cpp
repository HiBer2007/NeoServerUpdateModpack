#include "progress_card.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>

#include "animated_progress.h"

namespace HiBerGUI {

ProgressCard::ProgressCard(QWidget* parent)
    : QWidget(parent)
    , active_(false)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("progressCard"));
    setFixedWidth(360);
    setMinimumHeight(96);
    setMaximumHeight(150);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("progressCardFrame"));

    titleLabel_ = new QLabel(card);
    titleLabel_->setObjectName(QStringLiteral("progressCardTitle"));

    progress_ = new AnimatedProgress(card);
    progress_->setMinimumHeight(18);

    statusLabel_ = new QLabel(card);
    statusLabel_->setObjectName(QStringLiteral("progressCardStatus"));
    statusLabel_->setWordWrap(true);

    cancelButton_ = new QPushButton(QString::fromUtf8("\u53d6\u6d88"), card);
    cancelButton_->setObjectName(QStringLiteral("progressCardCancel"));
    cancelButton_->setFixedWidth(64);
    connect(cancelButton_, &QPushButton::clicked, this,
        &ProgressCard::cancelRequested);

    auto* titleRow = new QHBoxLayout;
    titleRow->addWidget(titleLabel_, 1);
    titleRow->addWidget(cancelButton_);

    auto* lay = new QVBoxLayout(card);
    lay->setContentsMargins(14, 10, 14, 12);
    lay->setSpacing(8);
    lay->addLayout(titleRow);
    lay->addWidget(progress_);
    lay->addWidget(statusLabel_);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(card);

    effect_ = new QGraphicsOpacityEffect(this);
    effect_->setOpacity(0.0);
    card->setGraphicsEffect(effect_);

    opacityAnim_ = new QPropertyAnimation(effect_, "opacity", this);
    opacityAnim_->setDuration(180);

    applyStyle();
    hide();
}

void ProgressCard::showCard(const QString& title, bool cancelable)
{
    titleLabel_->setText(title);
    cancelButton_->setVisible(cancelable);
    progress_->setValue(0);
    statusLabel_->setText(QString::fromUtf8("\u6b63\u5728\u5904\u7406\u2026"));
    active_ = true;

    raise();
    show();
    opacityAnim_->stop();
    opacityAnim_->setStartValue(0.0);
    opacityAnim_->setEndValue(1.0);
    opacityAnim_->start();
}

void ProgressCard::setProgress(int percent, const QString& status)
{
    if (!active_) return;
    progress_->setValue(percent);
    if (!status.isEmpty()) {
        statusLabel_->setText(status);
    }
}

void ProgressCard::complete(const QString& status)
{
    if (!active_) return;
    progress_->setValue(100);
    statusLabel_->setText(status.isEmpty()
        ? QString::fromUtf8("\u5b8c\u6210")
        : status);
}

void ProgressCard::fail(const QString& status)
{
    if (!active_) return;
    statusLabel_->setText(status.isEmpty()
        ? QString::fromUtf8("\u5931\u8d25")
        : status);
}

void ProgressCard::hideCard()
{
    if (!active_) return;
    active_ = false;
    opacityAnim_->stop();
    opacityAnim_->setStartValue(1.0);
    opacityAnim_->setEndValue(0.0);
    QObject::disconnect(opacityAnim_, &QPropertyAnimation::finished,
        this, nullptr);
    connect(opacityAnim_, &QPropertyAnimation::finished, this, [this]() {
        hide();
    });
    opacityAnim_->start();
}

bool ProgressCard::isActive() const
{
    return active_;
}

void ProgressCard::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        #progressCardFrame {
            background-color: #2b2f36;
            border: 1px solid #454b54;
            border-radius: 10px;
        }
        #progressCardTitle {
            color: #e8eaed;
            font-size: 13px;
            font-weight: bold;
        }
        #progressCardStatus {
            color: #9aa0a8;
            font-size: 11px;
        }
        #progressCardCancel {
            color: #e8eaed;
            background-color: #454b54;
            border: none;
            border-radius: 4px;
            padding: 3px 8px;
        }
        #progressCardCancel:hover {
            background-color: #565d68;
        }
    )"));
}

} // namespace HiBerGUI

#include "progress_card.moc"
