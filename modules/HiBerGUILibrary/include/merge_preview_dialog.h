#pragma once

#include <QDialog>

#include "code_editor_interface.h"

class QLabel;

namespace HiBerGUI {

class ICodeEditor;

// 通用 merge 预览窗口: 信息栏 + 代码编辑器 (Qt/Web 版可选) + 关闭按钮
class MergePreviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit MergePreviewDialog(CodeEditorKind kind, QWidget* parent = nullptr);
    ~MergePreviewDialog() override;

    // content = merge 结果文本; info = 顶部说明; langId = 高亮语言;
    // highlights = 区域背景标记 (如追踪值传播行, 可扩展)
    void setContent(const QString& content, const QString& info,
        const QString& langId,
        const QVector<RegionHighlight>& highlights = {});

    ICodeEditor* editor() const { return editor_; }

private:
    ICodeEditor* editor_ = nullptr;
    QLabel* infoLabel_ = nullptr;
};

} // namespace HiBerGUI
