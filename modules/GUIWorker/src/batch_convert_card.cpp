#include "batch_convert_card.h"

#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPaintEvent>

namespace GUIWorker {

namespace {

// 条形分布图: 已完成(绿) / 失败(红) / 剩余(灰) 三段
class BarChartWidget : public QWidget {
public:
    explicit BarChartWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(16);
    }

    void setValues(int done, int failed, int total)
    {
        done_ = done;
        failed_ = failed;
        total_ = total;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const int w = width();
        const int h = height();
        const int y = (h - 10) / 2;
        const int remain = qMax(0, total_ - done_ - failed_);

        auto drawSegment = [&](int x0, int seg, const QColor& c) {
            if (seg <= 0) return;
            p.setBrush(c);
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRectF(x0, y, seg - 1.0, 10), 3, 3);
        };

        int x = 0;
        if (total_ > 0) {
            const int doneW = done_ * w / qMax(1, total_);
            const int failW = failed_ * w / qMax(1, total_);
            drawSegment(x, doneW, QColor(0x7e, 0xcf, 0x8a));
            x += doneW;
            drawSegment(x, failW, QColor(0xe5, 0x73, 0x73));
            x += failW;
            drawSegment(x, remain * w / qMax(1, total_), QColor(0x3a, 0x41, 0x4b));
        } else {
            drawSegment(0, w, QColor(0x3a, 0x41, 0x4b));
        }
        Q_UNUSED(remain);
    }

private:
    int done_ = 0;
    int failed_ = 0;
    int total_ = 0;
};

QString cardStyleSheet()
{
    return QStringLiteral(R"(
        QFrame#bccFrame {
            background-color: #2b2f36;
            border: 1px solid #454b54;
            border-radius: 10px;
        }
        QLabel#bccTitle {
            color: #e8eaed;
            font-size: 14px;
            font-weight: bold;
        }
        QLabel#bccCounts {
            color: #9aa0a8;
            font-size: 12px;
        }
        QLabel#bccStatus {
            color: #c8ccd2;
            font-size: 12px;
        }
        QLabel#bccSummary {
            color: #ffd54f;
            font-size: 12px;
        }
        QPushButton#bccCancel, QPushButton#bccClose {
            color: #e8eaed;
            background-color: #454b54;
            border: none;
            border-radius: 4px;
            padding: 4px 12px;
        }
        QPushButton#bccCancel:hover, QPushButton#bccClose:hover {
            background-color: #565d68;
        }
        QScrollArea#bccScroll {
            border: 1px solid #3a414b;
            border-radius: 6px;
            background: #24282e;
        }
        QLabel#bccEmpty {
            color: #8a9099;
            font-size: 12px;
        }
    )");
}

} // namespace

BatchConvertCard::BatchConvertCard()
    : QWidget(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(cardStyleSheet());
    buildCard();
    hide();
}

void BatchConvertCard::buildCard()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addStretch(1);

    auto* card = new QFrame(this);
    card->setObjectName(QStringLiteral("bccFrame"));
    card->setFixedWidth(460);
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(16, 14, 16, 14);
    cardLay->setSpacing(10);

    auto* titleRow = new QHBoxLayout;
    titleLabel_ = new QLabel(card);
    titleLabel_->setObjectName(QStringLiteral("bccTitle"));
    cancelButton_ = new QPushButton(
        QString::fromUtf8("\u53d6\u6d88"), card);
    cancelButton_->setObjectName(QStringLiteral("bccCancel"));
    cancelButton_->setFixedWidth(64);
    connect(cancelButton_, &QPushButton::clicked, this,
        &BatchConvertCard::cancelRequested);
    titleRow->addWidget(titleLabel_, 1);
    titleRow->addWidget(cancelButton_);
    cardLay->addLayout(titleRow);

    modeStack_ = new QStackedWidget(card);

    // 转换中: 条形分布图 + 计数 + 当前操作
    progressPage_ = new QWidget(modeStack_);
    auto* ppLay = new QVBoxLayout(progressPage_);
    ppLay->setContentsMargins(0, 0, 0, 0);
    ppLay->setSpacing(8);
    barChart_ = new BarChartWidget(progressPage_);
    ppLay->addWidget(barChart_);
    countsLabel_ = new QLabel(progressPage_);
    countsLabel_->setObjectName(QStringLiteral("bccCounts"));
    ppLay->addWidget(countsLabel_);
    statusLabel_ = new QLabel(progressPage_);
    statusLabel_->setObjectName(QStringLiteral("bccStatus"));
    statusLabel_->setWordWrap(true);
    ppLay->addWidget(statusLabel_);
    modeStack_->addWidget(progressPage_);

    // 报告: 汇总 + 滚动列表 + 关闭
    reportPage_ = new QWidget(modeStack_);
    auto* rpLay = new QVBoxLayout(reportPage_);
    rpLay->setContentsMargins(0, 0, 0, 0);
    rpLay->setSpacing(8);
    reportSummaryLabel_ = new QLabel(reportPage_);
    reportSummaryLabel_->setObjectName(QStringLiteral("bccSummary"));
    reportSummaryLabel_->setWordWrap(true);
    rpLay->addWidget(reportSummaryLabel_);
    reportScroll_ = new QScrollArea(reportPage_);
    reportScroll_->setObjectName(QStringLiteral("bccScroll"));
    reportScroll_->setWidgetResizable(true);
    reportScroll_->setFixedHeight(180);
    reportListHost_ = new QWidget(reportScroll_);
    reportScroll_->setWidget(reportListHost_);
    rpLay->addWidget(reportScroll_);
    auto* closeRow = new QHBoxLayout;
    closeRow->addStretch(1);
    closeButton_ = new QPushButton(
        QString::fromUtf8("\u5173\u95ed"), reportPage_);
    closeButton_->setObjectName(QStringLiteral("bccClose"));
    connect(closeButton_, &QPushButton::clicked, this, [this]() {
        dismiss();
        emit closed();
    });
    closeRow->addWidget(closeButton_);
    rpLay->addLayout(closeRow);
    modeStack_->addWidget(reportPage_);

    cardLay->addWidget(modeStack_);

    outer->addWidget(card, 0, Qt::AlignHCenter);
    outer->addStretch(1);
}

void BatchConvertCard::attachTo(QWidget* host)
{
    host_ = host ? host : window();
    if (host_ && host_ != this) {
        setParent(host_);
        host_->installEventFilter(this);
        setGeometry(host_->rect());
    }
}

void BatchConvertCard::reattachToWindow()
{
    if (!host_) {
        attachTo(window());
    }
    if (!host_ || host_ == this) return;
    setGeometry(host_->rect());
    raise();
}

void BatchConvertCard::begin(const QString& title, int total)
{
    total_ = qMax(0, total);
    reattachToWindow();
    titleLabel_->setText(title);
    static_cast<BarChartWidget*>(barChart_)->setValues(0, 0, total_);
    countsLabel_->setText(QString::fromUtf8(
        "\u5df2\u5b8c\u6210 0 \u00b7 \u5931\u8d25 0 \u00b7 \u5269\u4f59 %1")
        .arg(total_));
    statusLabel_->setText(QString::fromUtf8("\u6b63\u5728\u51c6\u5907\u2026"));
    modeStack_->setCurrentWidget(progressPage_);
    cancelButton_->setVisible(true);
    active_ = true;
    show();
    raise();
}

void BatchConvertCard::setProgress(int done, int failed, const QString& current)
{
    if (!active_) return;
    static_cast<BarChartWidget*>(barChart_)->setValues(done, failed, total_);
    const int remain = qMax(0, total_ - done - failed);
    countsLabel_->setText(QString::fromUtf8(
        "\u5df2\u5b8c\u6210 %1 \u00b7 \u5931\u8d25 %2 \u00b7 \u5269\u4f59 %3")
        .arg(done).arg(failed).arg(remain));
    if (!current.isEmpty()) {
        statusLabel_->setText(current);
    }
}

void BatchConvertCard::showReport(const QVector<BatchConvertResult>& results,
    const QString& summary, bool cancelled)
{
    if (!active_) return;
    Q_UNUSED(cancelled);
    reportSummaryLabel_->setText(summary);

    if (reportListHost_->layout()) {
        delete reportListHost_->layout();
    }
    auto* listLay = new QVBoxLayout(reportListHost_);
    listLay->setContentsMargins(8, 8, 8, 8);
    listLay->setSpacing(2);

    if (results.isEmpty()) {
        auto* empty = new QLabel(
            QString::fromUtf8("\u65e0\u53ef\u5c55\u793a\u7684\u7ed3\u679c\u3002"),
            reportListHost_);
        empty->setObjectName(QStringLiteral("bccEmpty"));
        listLay->addWidget(empty);
    } else {
        int okCount = 0;
        int failCount = 0;
        for (const BatchConvertResult& r : results) {
            if (r.ok) ++okCount;
            else ++failCount;
            auto* row = new QLabel(reportListHost_);
            row->setWordWrap(true);
            row->setTextInteractionFlags(Qt::TextSelectableByMouse);
            if (r.ok) {
                row->setText(QString::fromUtf8("\u2713 %1").arg(r.name));
                row->setStyleSheet(QStringLiteral("color: #7ecf8a; font-size: 12px;"));
            } else {
                row->setText(QString::fromUtf8("\u2717 %1 \u2014 %2")
                    .arg(r.name, r.reason));
                row->setStyleSheet(QStringLiteral("color: #ffd54f; font-size: 12px;"));
            }
            listLay->addWidget(row);
        }
        if (reportSummaryLabel_->text().isEmpty()) {
            reportSummaryLabel_->setText(QString::fromUtf8(
                "\u5df2\u5b8c\u6210 %1 \u00b7 \u5931\u8d25 %2")
                .arg(okCount).arg(failCount));
        }
    }
    listLay->addStretch(1);

    cancelButton_->setVisible(false);
    modeStack_->setCurrentWidget(reportPage_);
    active_ = true;
}

void BatchConvertCard::dismiss()
{
    active_ = false;
    hide();
}

void BatchConvertCard::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    // 半透明灰色遮罩
    QPainter p(this);
    p.fillRect(rect(), QColor(15, 17, 20, 160));
}

void BatchConvertCard::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

bool BatchConvertCard::eventFilter(QObject* watched, QEvent* event)
{
    // 宿主窗口尺寸变化时遮罩跟随
    if (watched == host_ && event->type() == QEvent::Resize) {
        if (active_ && host_) {
            setGeometry(host_->rect());
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace GUIWorker

#include "batch_convert_card.moc"
