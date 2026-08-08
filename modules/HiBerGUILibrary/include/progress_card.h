#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

namespace HiBerGUI {

class AnimatedProgress;

class ProgressCard : public QWidget {
    Q_OBJECT

public:
    explicit ProgressCard(QWidget* parent = nullptr);

    void showCard(const QString& title, bool cancelable);
    void setProgress(int percent, const QString& status);
    void complete(const QString& status);
    void fail(const QString& status);
    void hideCard();
    bool isActive() const;

signals:
    void cancelRequested();

private:
    QLabel* titleLabel_;
    QLabel* statusLabel_;
    AnimatedProgress* progress_;
    QPushButton* cancelButton_;
    QGraphicsOpacityEffect* effect_;
    QPropertyAnimation* opacityAnim_;
    bool active_;

    void applyStyle();
};

} // namespace HiBerGUI
