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

    int value() const;
    QString text() const;

    void startAnimation();
    void stopAnimation();

    int animatedValue() const { return animatedValue_; }
    void setAnimatedValue(int val);

private slots:
    void onAnimationTick();

private:
    QProgressBar* bar_;
    QLabel* textLabel_;
    QTimer* animTimer_;
    QTimer* pulseTimer_;
    QPropertyAnimation* smoothAnim_;
    int animatedValue_;
    int targetValue_;
    bool indeterminate_;
    int pulseDirection_;

    void applyStyle();
    void smoothToValue(int value);
};

} // namespace HiBerGUI

