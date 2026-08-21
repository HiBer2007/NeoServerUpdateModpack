#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>

namespace HiBerGUI {

// 紧凑工作卡片 (右上角任务进度): 尖角、标题行 + 状态文字 + 底部贴边进度条。
// 与 ProgressCard (仓库加载大卡) 分离; 无论是否可视都持续刷新。
class WorkCard : public QWidget {
    Q_OBJECT

public:
    explicit WorkCard(QWidget* parent = nullptr);

    void showCard(const QString& title, bool cancelable);
    void setProgress(int percent, const QString& status);
    void complete(const QString& status);
    void fail(const QString& status);
    bool isActive() const { return active_; }

signals:
    void cancelRequested();

private:
    QLabel* titleLabel_;
    QLabel* statusLabel_;
    QProgressBar* bar_;
    QPushButton* cancelButton_;
    bool active_ = false;

    void applyStyle();
};

} // namespace HiBerGUI
