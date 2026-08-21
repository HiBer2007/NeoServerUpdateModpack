#include "toast_notification.h"

#include <QVBoxLayout>
#include <QPalette>
#include <QColor>
#include <QEasingCurve>
#include <QEnterEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QAbstractAnimation>

namespace HiBerGUI {

ToastNotification::ToastNotification(QWidget* parent)
    : QWidget(parent)
    , totalMs_(3000)
    , remainingMs_(3000)
    , hovered_(false)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* card = new QWidget(this);
    card->setObjectName(QStringLiteral("toastCard"));
    card->setStyleSheet(QStringLiteral(
        "#toastCard {"
        "  background: rgba(200, 40, 40, 0.65);"
        "  border: 1px solid rgba(255, 120, 120, 0.45);"
        "}"));

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(8, 6, 8, 6);
    cardLayout->setSpacing(3);

    titleLabel_ = new QLabel(card);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(13);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    titleLabel_->setStyleSheet(QStringLiteral("color: #ffffff;"));
    titleLabel_->setWordWrap(true);
    cardLayout->addWidget(titleLabel_);

    detailLabel_ = new QLabel(card);
    detailLabel_->setStyleSheet(QStringLiteral("color: rgba(255, 235, 235, 0.95); font-size: 12px;"));
    detailLabel_->setWordWrap(true);
    cardLayout->addWidget(detailLabel_);

    outer->addWidget(card);

    countdownBar_ = new QProgressBar(this);
    countdownBar_->setRange(0, 1000);
    countdownBar_->setValue(1000);
    countdownBar_->setTextVisible(false);
    countdownBar_->setFixedHeight(3);
    countdownBar_->setStyleSheet(QStringLiteral(
        "QProgressBar { background: rgba(120, 20, 20, 0.9); border: none; }"
        "QProgressBar::chunk { background: #ff6b6b; }"));
    outer->addWidget(countdownBar_);

    countdownAnim_ = new QPropertyAnimation(countdownBar_, "value", this);
    countdownAnim_->setDuration(0);
    countdownAnim_->setEasingCurve(QEasingCurve::Linear);

    slideIn_ = new QPropertyAnimation(this, "pos", this);
    slideIn_->setDuration(220);
    slideIn_->setEasingCurve(QEasingCurve::OutCubic);
    connect(slideIn_, &QPropertyAnimation::finished, this, [this]() {
        sliding_ = false;
    });

    slideOut_ = new QPropertyAnimation(this, "pos", this);
    slideOut_->setDuration(180);
    slideOut_->setEasingCurve(QEasingCurve::InCubic);
    connect(slideOut_, &QPropertyAnimation::finished, this, [this]() {
        sliding_ = false;
        hide();
        deleteLater();
    });

    hide();
}

QPoint ToastNotification::targetPos() const
{
    QWidget* p = parentWidget();
    int pw = p ? p->width() : 800;
    return QPoint(pw - width() - 16, 16 + topOffset_);
}

QPoint ToastNotification::startPos() const
{
    QWidget* p = parentWidget();
    int pw = p ? p->width() : 800;
    return QPoint(pw, 16 + topOffset_);
}

void ToastNotification::showError(const QString& title, const QString& detail,
                                  int timeoutMs)
{
    titleLabel_->setText(title);
    detailLabel_->setText(detail);
    detailLabel_->setVisible(!detail.isEmpty());
    totalMs_ = timeoutMs;
    remainingMs_ = timeoutMs;
    hovered_ = false;
    sliding_ = false;

    countdownBar_->setRange(0, qMax(totalMs_, 1));
    countdownBar_->setValue(totalMs_);

    reflow();

    move(startPos());
    show();
    raise();

    slideIn_->stop();
    slideOut_->stop();
    slideIn_->setStartValue(startPos());
    slideIn_->setEndValue(targetPos());
    slideIn_->start();
    sliding_ = true;

    startCountdown();
}

void ToastNotification::dismiss()
{
    stopCountdown();
    if (isVisible() && !sliding_) {
        sliding_ = true;
        slideOut_->setStartValue(pos());
        slideOut_->setEndValue(startPos());
        slideOut_->start();
    }
}

void ToastNotification::enterEvent(QEnterEvent* event)
{
    hovered_ = true;
    stopCountdown();
    QWidget::enterEvent(event);
}

void ToastNotification::leaveEvent(QEvent* event)
{
    hovered_ = false;
    if (isVisible() && !sliding_ && remainingMs_ > 0) {
        startCountdown();
    }
    QWidget::leaveEvent(event);
}

void ToastNotification::startCountdown()
{
    if (countdownAnim_->state() == QAbstractAnimation::Running) return;
    int current = countdownBar_->value();
    if (current <= 0) return;
    countdownAnim_->stop();
    countdownAnim_->setStartValue(current);
    countdownAnim_->setEndValue(0);
    countdownAnim_->setDuration(current);
    countdownAnim_->start();
    connect(countdownAnim_, &QPropertyAnimation::finished, this,
        &ToastNotification::updateCountdown, Qt::UniqueConnection);
}

void ToastNotification::stopCountdown()
{
    if (countdownAnim_->state() == QAbstractAnimation::Running) {
        countdownAnim_->stop();
    }
    remainingMs_ = countdownBar_->value();
}

void ToastNotification::updateCountdown()
{
    remainingMs_ = countdownBar_->value();
    if (remainingMs_ <= 0) {
        dismiss();
        return;
    }
}

int ToastNotification::textColumns(const QString& text, const QFont& font) const
{
    QFontMetrics fm(font);
    int cols = 0;
    for (const QChar& ch : text) {
        cols += fm.horizontalAdvance(ch);
    }
    return cols;
}

void ToastNotification::reflow()
{
    ensurePolished();

    const int hPad = 16;   // cardLayout margins 8+8
    const int vPad = 12;   // cardLayout margins 6+6
    const int hBorder = 2; // card 边框 1+1
    const int vBorder = 2; // card 边框上下 1+1
    const int barH = 3;    // countdown bar
    const int spacing = 3; // cardLayout spacing

    QFontMetrics titleFm(titleLabel_->font());
    QFontMetrics detailFm(detailLabel_->font());

    int titleCols = textColumns(titleLabel_->text(), titleLabel_->font());
    int detailCols = detailLabel_->isVisible()
        ? textColumns(detailLabel_->text(), detailLabel_->font())
        : 0;
    int maxCols = qMax(titleCols, detailCols);

    const int minW = 240;
    const int maxW = 520;
    int cardW = qBound(minW, maxCols + hPad + hBorder, maxW);

    int availTextW = cardW - hPad - hBorder;
    int titleRectH = titleFm.boundingRect(
        QRect(0, 0, availTextW, 100000),
        Qt::TextWordWrap, titleLabel_->text()).height();
    int titleLines = qMax(1, (titleRectH + titleFm.height() - 1) / titleFm.height());
    int titleH = titleLines * titleFm.height() + 2;

    int detailH = 0;
    if (detailLabel_->isVisible()) {
        int detailRectH = detailFm.boundingRect(
            QRect(0, 0, availTextW, 100000),
            Qt::TextWordWrap, detailLabel_->text()).height();
        int detailLines = qMax(1, (detailRectH + detailFm.height() - 1) / detailFm.height());
        detailH = detailLines * detailFm.height() + 1;
    }

    int cardH = vPad + titleH;
    if (detailLabel_->isVisible()) {
        cardH += spacing + detailH;
    }
    const int barMargin = 4;
    int totalH = cardH + vBorder + barH + barMargin;

    setFixedSize(cardW, totalH);
}

} // namespace HiBerGUI

#include "toast_notification.moc"
