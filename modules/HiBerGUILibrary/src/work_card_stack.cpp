#include "work_card_stack.h"

#include <QEvent>
#include <QCursor>
#include <QEnterEvent>
#include <QPropertyAnimation>
#include <QEasingCurve>

namespace HiBerGUI {

WorkCardStack::WorkCardStack(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_StyledBackground, false);
    collapseTimer_ = new QTimer(this);
    collapseTimer_->setSingleShot(true);
    collapseTimer_->setInterval(120);
    connect(collapseTimer_, &QTimer::timeout, this, [this]() {
        if (hoveredIndex_ >= 0 && !cursorInsideStack()) {
            hoveredIndex_ = -1;
            updateLayout();
        }
    });
    if (parent) {
        parent->installEventFilter(this);
    }
    hide();
}

void WorkCardStack::reposition()
{
    QWidget* p = parentWidget();
    if (!p) return;
    move(p->width() - width() - 16, 16);
    raise();
}

WorkCard* WorkCardStack::addCard(const QString& title, bool cancelable)
{
    auto* card = new WorkCard(this);
    card->installEventFilter(this);
    card->showCard(title, cancelable);
    cards_.push_back(card);
    updateLayout();
    show();
    raise();
    return card;
}

void WorkCardStack::removeCard(WorkCard* card)
{
    if (!card) return;
    cards_.removeAll(card);
    if (hoveredIndex_ >= cards_.size()) {
        hoveredIndex_ = -1;
    }
    card->hide();
    card->deleteLater();
    updateLayout();
    if (cards_.isEmpty()) {
        hide();
    }
}

void WorkCardStack::updateLayout()
{
    if (cards_.isEmpty()) {
        resize(0, 0);
        return;
    }

    // 默认: 卡 k 顶部 y = k*step (被上方卡盖住上半部分, 露出下半部分含进度条)
    // 悬浮卡 i: 卡 i 原位向下平移完整展开 (紧贴上方卡底部), 其下方卡片连带下移
    const int H = cards_.first()->height();
    const int step = qMax(8, H - RevealHeight);
    const int n = cards_.size();
    for (int i = 0; i < n; ++i) {
        int targetY;
        if (hoveredIndex_ >= 0 && i > hoveredIndex_) {
            targetY = i * step + (H - step);          // 下方连带下移
        } else if (i == hoveredIndex_) {
            targetY = (i > 0) ? (i - 1) * step + H : 0;  // 原位展开 (紧贴上方卡底部)
        } else {
            targetY = i * step;
        }
        animateCardTo(cards_[i], QPoint(0, targetY));
    }
    // z-order: 上方卡在顶层 (先 raise 底层)
    for (int i = n - 1; i >= 0; --i) {
        cards_[i]->raise();
    }

    resize(cards_.first()->width(), H + (n - 1) * step);
    // 尺寸变化后立即重新锚定, 防止右缘溢出宿主
    reposition();
}

void WorkCardStack::animateCardTo(WorkCard* card, const QPoint& target)
{
    if (card->pos() == target) return;
    auto* anim = new QPropertyAnimation(card, "pos", this);
    anim->setDuration(160);
    anim->setEasingCurve(QEasingCurve::OutCubic);   // 加速减速
    anim->setStartValue(card->pos());
    anim->setEndValue(target);
    QObject::connect(anim, &QPropertyAnimation::finished, anim,
        &QObject::deleteLater);
    anim->start();
}

bool WorkCardStack::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == parentWidget() && ev->type() == QEvent::Resize) {
        // 宿主尺寸变化: 重新锚定
        reposition();
        return false;
    }
    if (ev->type() == QEvent::Enter) {
        // 悬浮某张卡: 平移到顶层完整显示, 其余连带平移
        if (auto* card = qobject_cast<WorkCard*>(obj)) {
            const int idx = cards_.indexOf(card);
            if (idx >= 0 && hoveredIndex_ != idx) {
                hoveredIndex_ = idx;
                updateLayout();
            }
        }
        collapseTimer_->stop();
        return false;
    }
    if (ev->type() == QEvent::Leave) {
        collapseSoon();
        return false;
    }
    return QWidget::eventFilter(obj, ev);
}

bool WorkCardStack::cursorInsideStack() const
{
    const QPoint gp = QCursor::pos();
    const QRect r(geometry());
    return r.contains(mapFromGlobal(gp));
}

void WorkCardStack::collapseSoon()
{
    if (hoveredIndex_ >= 0) {
        collapseTimer_->start();
    }
}

} // namespace HiBerGUI

#include "work_card_stack.moc"
