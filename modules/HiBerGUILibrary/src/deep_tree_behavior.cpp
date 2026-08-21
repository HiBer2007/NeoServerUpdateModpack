#include "deep_tree_behavior.h"

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QEvent>
#include <QMouseEvent>
#include <QRect>

#include <functional>

namespace {

bool branchIndicatorHit(const QTreeWidget* tree, const QPoint& pos,
    QTreeWidgetItem* item)
{
    if (!item) return false;
    const QRect r = tree->visualItemRect(item);
    // 折叠三角位于行首 indentation() 宽的区域
    return QRect(r.left(), r.top(), tree->indentation(), r.height())
        .contains(pos);
}

} // namespace

namespace HiBerGUI {

DeepTreeBehavior::DeepTreeBehavior(QTreeWidget* tree, QObject* parent)
    : QObject(parent)
    , tree_(tree)
{
    // 双击与折叠三角均由本行为在 viewport 事件过滤器统一接管:
    // consume 事件 -> 与 Qt 内部点击/双击序列隔离, 展开动画不被吞
    tree_->setExpandsOnDoubleClick(false);
    tree_->viewport()->installEventFilter(this);
}

bool DeepTreeBehavior::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == tree_->viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QPoint pos = me->position().toPoint();
                QTreeWidgetItem* item = tree_->itemAt(pos);
                if (item && item->childCount() > 0
                    && branchIndicatorHit(tree_, pos, item)) {
                    toggleItem(item);
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                const QPoint pos = me->position().toPoint();
                QTreeWidgetItem* item = tree_->itemAt(pos);
                if (item && item->childCount() > 0) {
                    // 双击落在折叠三角区时, 第一次 press 已切换, 直接消费防二次切换
                    if (!branchIndicatorHit(tree_, pos, item)) {
                        toggleItem(item);
                    }
                    return true;
                }
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void DeepTreeBehavior::toggleItem(QTreeWidgetItem* item)
{
    if (!item) return;
    if (item->treeWidget() != tree_) return;
    if (item->childCount() == 0) return;

    if (item->isExpanded()) {
        collapseDeep(item);
    } else {
        // 展开一层 (setAnimated(true) 提供动画)
        item->setExpanded(true);
    }
}

void DeepTreeBehavior::collapseDeep(QTreeWidgetItem* item)
{
    // 先折叠父项: 整个子树收起, setAnimated(true) 提供整体收起动画
    item->setExpanded(false);
    // 再清空全部后代的展开标记, 使再次展开时仅显示一层
    std::function<void(QTreeWidgetItem*)> clearSub =
        [&](QTreeWidgetItem* it) {
            for (int i = 0; i < it->childCount(); ++i) {
                QTreeWidgetItem* c = it->child(i);
                if (c->childCount() > 0) {
                    clearSub(c);
                }
                c->setExpanded(false);
            }
        };
    clearSub(item);
}

} // namespace HiBerGUI

#include "deep_tree_behavior.moc"