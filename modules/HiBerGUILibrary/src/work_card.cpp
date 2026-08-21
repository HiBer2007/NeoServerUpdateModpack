#include "work_card.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

namespace HiBerGUI {

WorkCard::WorkCard(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName(QStringLiteral("workCard"));
    setFixedWidth(300);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 4, 8, 0);   // 底部 0: 进度条贴边
    lay->setSpacing(2);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(6);
    titleLabel_ = new QLabel(this);
    titleLabel_->setObjectName(QStringLiteral("workCardTitle"));
    cancelButton_ = new QPushButton(QString::fromUtf8("\u53d6\u6d88"), this);
    cancelButton_->setObjectName(QStringLiteral("workCardCancel"));
    cancelButton_->setFixedSize(36, 18);
    titleRow->addWidget(titleLabel_, 1);
    titleRow->addWidget(cancelButton_);
    lay->addLayout(titleRow);

    statusLabel_ = new QLabel(QString::fromUtf8("\u6b63\u5728\u5904\u7406\u2026"), this);
    statusLabel_->setObjectName(QStringLiteral("workCardStatus"));
    statusLabel_->setWordWrap(false);
    lay->addWidget(statusLabel_);

    // 底部贴边进度条: 无外边距, 高 4px
    bar_ = new QProgressBar(this);
    bar_->setObjectName(QStringLiteral("workCardBar"));
    bar_->setRange(0, 100);
    bar_->setValue(0);
    bar_->setTextVisible(false);
    bar_->setFixedHeight(4);
    lay->addWidget(bar_);

    connect(cancelButton_, &QPushButton::clicked, this,
        &WorkCard::cancelRequested);

    applyStyle();
    hide();
}

void WorkCard::showCard(const QString& title, bool cancelable)
{
    titleLabel_->setText(title);
    cancelButton_->setVisible(cancelable);
    bar_->setValue(0);
    statusLabel_->setText(QString::fromUtf8("\u6b63\u5728\u5904\u7406\u2026"));
    active_ = true;
    show();
    raise();
}

void WorkCard::setProgress(int percent, const QString& status)
{
    if (!active_) return;
    // 无论卡片当前是否可视 (被折叠遮挡) 都持续刷新
    bar_->setValue(qBound(0, percent, 100));
    if (!status.isEmpty()) {
        statusLabel_->setText(status);
    }
}

void WorkCard::complete(const QString& status)
{
    if (!active_) return;
    bar_->setValue(100);
    statusLabel_->setText(status.isEmpty()
        ? QString::fromUtf8("\u5b8c\u6210")
        : status);
}

void WorkCard::fail(const QString& status)
{
    if (!active_) return;
    statusLabel_->setText(status.isEmpty()
        ? QString::fromUtf8("\u5931\u8d25")
        : status);
}

void WorkCard::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QWidget#workCard {
            background-color: #2b2f36;
            border: 1px solid #454b54;
            border-radius: 2px;
        }
        QLabel#workCardTitle {
            color: #e8eaed;
            font-size: 13px;
            font-weight: bold;
        }
        QLabel#workCardStatus {
            color: #9aa0a8;
            font-size: 11px;
        }
        QPushButton#workCardCancel {
            color: #d8dce2;
            background-color: #454b54;
            border: none;
            border-radius: 2px;
            font-size: 10px;
            padding: 0;
        }
        QPushButton#workCardCancel:hover {
            background-color: #565d68;
        }
        QProgressBar#workCardBar {
            border: none;
            background-color: #3a4048;
        }
        QProgressBar#workCardBar::chunk {
            background-color: #0078d4;
        }
    )"));
}

} // namespace HiBerGUI

#include "work_card.moc"
