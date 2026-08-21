#pragma once

#include <QWidget>
#include <QVector>
#include <QString>

class QLabel;
class QPushButton;
class QStackedWidget;
class QScrollArea;

namespace GUIWorker {

struct BatchConvertResult {
    QString name;
    bool ok = false;
    QString reason;
};

// 批量转换模态卡片: 半透明灰色遮罩覆盖宿主窗口并阻止交互 + 居中卡片。
// 转换中: 条形分布图 (已完成绿 / 失败红 / 剩余灰) + 当前操作 + 取消按钮;
// 结束: 报告 (已完成/失败计数 + 操作提示 + 详细列表, 成功绿/失败黄) + 关闭按钮。
class BatchConvertCard : public QWidget {
    Q_OBJECT

public:
    // 无父构造; 必须 attachTo(宿主窗口) 后 begin, 卡片作为宿主子控件显示
    explicit BatchConvertCard();
    void attachTo(QWidget* host);

    void begin(const QString& title, int total);
    void setProgress(int done, int failed, const QString& current);
    void showReport(const QVector<BatchConvertResult>& results,
        const QString& summary, bool cancelled);
    bool isActive() const { return active_; }
    void dismiss();

signals:
    void cancelRequested();
    void closed();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override {}
    void mouseMoveEvent(QMouseEvent* event) override {}
    void mouseReleaseEvent(QMouseEvent* event) override {}
    void keyPressEvent(QKeyEvent* event) override {}
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void reattachToWindow();
    void buildCard();

    QWidget* host_ = nullptr;
    bool active_ = false;

    QLabel* titleLabel_;
    QWidget* barChart_;
    QLabel* countsLabel_;
    QLabel* statusLabel_;
    QPushButton* cancelButton_;
    QStackedWidget* modeStack_;
    QWidget* progressPage_;
    QWidget* reportPage_;
    QLabel* reportSummaryLabel_;
    QScrollArea* reportScroll_;
    QWidget* reportListHost_;
    QPushButton* closeButton_;

    int total_ = 0;
};

} // namespace GUIWorker
