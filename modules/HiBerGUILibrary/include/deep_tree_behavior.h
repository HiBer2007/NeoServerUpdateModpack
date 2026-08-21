#pragma once

#include <QObject>

class QTreeWidget;
class QTreeWidgetItem;
class QEvent;

namespace HiBerGUI {

// 附加到 QTreeWidget 的展开/折叠行为:
// 点击折叠三角或双击目录项时, 展开仅展开一层, 折叠连同所有子层级一并折叠。
// 展开/收起动画由 QTreeWidget::setAnimated(true) 提供 (本行为不改动该设置)。
class DeepTreeBehavior : public QObject {
    Q_OBJECT

public:
    explicit DeepTreeBehavior(QTreeWidget* tree, QObject* parent = nullptr);

    void toggleItem(QTreeWidgetItem* item);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void collapseDeep(QTreeWidgetItem* item);

    QTreeWidget* tree_ = nullptr;
};

} // namespace HiBerGUI