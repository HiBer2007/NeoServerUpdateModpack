#include "animated_progress.h"

#include <QVBoxLayout>
#include <QEasingCurve>

namespace HiBerGUI {

AnimatedProgress::AnimatedProgress(QWidget* parent)
    : QWidget(parent)
    , animatedValue_(0)
    , targetValue_(0)
    , indeterminate_(false)
    , pulseDirection_(1)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    textLabel_ = new QLabel("准备就绪", this);
    textLabel_->setAlignment(Qt::AlignLeft);
    textLabel_->setStyleSheet(
        "QLabel { color: #333; font-size: 12px; padding: 2px 4px; }");
    layout->addWidget(textLabel_);

    bar_ = new QProgressBar(this);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    bar_->setTextVisible(false);
    bar_->setMinimumHeight(22);
    bar_->setMaximumHeight(22);

    layout->addWidget(bar_);

    animTimer_ = new QTimer(this);
    animTimer_->setInterval(30);

    pulseTimer_ = new QTimer(this);
    pulseTimer_->setInterval(16);

    smoothAnim_ = new QPropertyAnimation(this, "animatedValue", this);
    smoothAnim_->setDuration(400);
    smoothAnim_->setEasingCurve(QEasingCurve::InOutCubic);

    connect(animTimer_, &QTimer::timeout,
        this, &AnimatedProgress::onAnimationTick);
    connect(smoothAnim_, &QPropertyAnimation::valueChanged, this, [this]() {
        bar_->setValue(animatedValue_);
    });

    applyStyle();
}

void AnimatedProgress::setValue(int percent)
{
    targetValue_ = qBound(0, percent, 100);

    if (indeterminate_) {
        setIndeterminate(false);
    }

    stopAnimation();

    if (targetValue_ == 100) {
        bar_->setValue(100);
        animatedValue_ = 100;
    } else {
        smoothToValue(targetValue_);
    }
}

void AnimatedProgress::setIndeterminate(bool on)
{
    indeterminate_ = on;
    if (on) {
        smoothAnim_->stop();
        bar_->setRange(0, 0);
        animatedValue_ = 0;
        pulseDirection_ = 1;
        pulseTimer_->start();
    } else {
        pulseTimer_->stop();
        bar_->setRange(0, 100);
        bar_->setValue(0);
    }
}

void AnimatedProgress::setText(const QString& text)
{
    textLabel_->setText(text);
}

int AnimatedProgress::value() const
{
    return bar_->value();
}

QString AnimatedProgress::text() const
{
    return textLabel_->text();
}

void AnimatedProgress::startAnimation()
{
    if (!indeterminate_) {
        bar_->setRange(0, 100);
    }
    animTimer_->start();
}

void AnimatedProgress::stopAnimation()
{
    animTimer_->stop();
}

void AnimatedProgress::setAnimatedValue(int val)
{
    animatedValue_ = val;
    if (!indeterminate_) {
        bar_->setValue(val);
    }
}

void AnimatedProgress::onAnimationTick()
{
    if (indeterminate_) return;

    if (animatedValue_ < targetValue_) {
        animatedValue_ += 1;
        if (animatedValue_ > targetValue_) {
            animatedValue_ = targetValue_;
        }
        bar_->setValue(animatedValue_);
    }

    if (animatedValue_ >= targetValue_) {
        stopAnimation();
    }
}

void AnimatedProgress::smoothToValue(int value)
{
    smoothAnim_->stop();
    smoothAnim_->setStartValue(animatedValue_);
    smoothAnim_->setEndValue(value);
    smoothAnim_->start();
}

void AnimatedProgress::applyStyle()
{
    bar_->setStyleSheet(
        "QProgressBar {"
        "  border: 1px solid #bbb;"
        "  border-radius: 4px;"
        "  background-color: #e0e0e0;"
        "  text-align: center;"
        "  color: #333;"
        "}"
        "QProgressBar::chunk {"
        "  background-color: qlineargradient("
        "    x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #0078d4,"
        "    stop:0.5 #00a0e8,"
        "    stop:1 #0078d4"
        "  );"
        "  border-radius: 3px;"
        "  margin: 1px;"
        "}");
}

} // namespace HiBerGUI

#include "animated_progress.moc"


