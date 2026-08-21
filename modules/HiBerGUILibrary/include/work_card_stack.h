#pragma once

#include <QWidget>
#include <QList>
#include <QTimer>

#include "work_card.h"

namespace HiBerGUI {

// 层叠工作卡片堆 (右上角): 卡片保持完整展开形态 (无折叠样式),
// 最顶层完整显示, 其余卡片被压在下层露出下半部分 (进度条);
// 悬浮某张卡 = 平移到顶层 (其余连带平移); 移动为平滑动画 (加速减速)。
// 组件自锚定宿主右上角: 尺寸/宿主变化时自动重新定位 (卡片为子组件)。
class WorkCardStack : public QWidget {
    Q_OBJECT

public:
    explicit WorkCardStack(QWidget* parent = nullptr);

    WorkCard* addCard(const QString& title, bool cancelable);
    void removeCard(WorkCard* card);
    void updateLayout();
    // 重新锚定到宿主右上角 (尺寸或宿主变化后调用)
    void reposition();
    bool isEmpty() const { return cards_.isEmpty(); }
    int count() const { return cards_.size(); }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    bool cursorInsideStack() const;
    void collapseSoon();
    void animateCardTo(WorkCard* card, const QPoint& target);

    QList<WorkCard*> cards_;
    int hoveredIndex_ = -1;   // -1 = 无悬浮
    QTimer* collapseTimer_ = nullptr;

    // 露出高度: 被盖住的卡露出下半部分 (含底部进度条)
    static const int RevealHeight = 22;
};

} // namespace HiBerGUI
