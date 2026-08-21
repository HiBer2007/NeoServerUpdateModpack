#pragma once

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QPropertyAnimation>
#include <QFont>

namespace HiBerGUI {

class ToastNotification : public QWidget {
    Q_OBJECT

public:
    explicit ToastNotification(QWidget* parent);

    void showError(const QString& title, const QString& detail,
                   int timeoutMs = 3000);
    void dismiss();
    // 顶部偏移: 使 toast 显示在卡片堆下方/上方 (宿主按卡片堆高度设置)
    void setTopOffset(int y) { topOffset_ = y; }

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void startCountdown();
    void stopCountdown();
    QPoint targetPos() const;
    QPoint startPos() const;
    void updateCountdown();
    void reflow();
    int textColumns(const QString& text, const QFont& font) const;

    QLabel* titleLabel_;
    QLabel* detailLabel_;
    QProgressBar* countdownBar_;
    QPropertyAnimation* countdownAnim_;
    QPropertyAnimation* slideIn_;
    QPropertyAnimation* slideOut_;

    int totalMs_;
    int remainingMs_;
    bool hovered_;
    bool sliding_ = false;
    int topOffset_ = 0;
};

} // namespace HiBerGUI
