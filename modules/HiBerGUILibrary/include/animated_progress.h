#pragma once

#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QPropertyAnimation>

namespace HiBerGUI {

class AnimatedProgress : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int animatedValue READ animatedValue WRITE setAnimatedValue)

public:
    explicit AnimatedProgress(QWidget* parent = nullptr);

    void setValue(int percent);
    void setIndeterminate(bool on);
    void setText(const QString& text);

    // 紧凑模式: 隐藏文字, 进度条收窄为细条 (常驻显示用)
    void setCompact(bool on);

    int value() const;
    QString text() const;

    void startAnimation();
    void stopAnimation();

    int animatedValue() const { return animatedValue_; }
    void setAnimatedValue(int val);

private slots:
    void onAnimationTick();

private:
    bool event(QEvent* e) override;

    QProgressBar* bar_;
    QLabel* textLabel_;
    QTimer* animTimer_;
    QTimer* pulseTimer_;
    QPropertyAnimation* smoothAnim_;
    int animatedValue_;
    int targetValue_;
    bool indeterminate_;
    bool pulseDirection_;
    bool compact_;

    void applyStyle();
    void smoothToValue(int value);
};

} // namespace HiBerGUI

