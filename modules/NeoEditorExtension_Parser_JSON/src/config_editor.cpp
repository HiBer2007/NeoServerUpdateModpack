#include "config_editor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>

ConfigEditor::ConfigEditor(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);

    auto* modeGroup = new QGroupBox("同步模式", this);
    auto* modeLayout = new QHBoxLayout(modeGroup);
    modeCombo_ = new QComboBox(modeGroup);
    modeCombo_->addItems({"full_sync - 完整同步", "config_merge - 配置合并",
        "line_by_line - 逐行同步", "no_sync - 不同步"});
    modeLayout->addWidget(modeCombo_);
    layout->addWidget(modeGroup);

    auto* contentGroup = new QGroupBox("文件内容 (只读)", this);
    auto* contentLayout = new QVBoxLayout(contentGroup);
    contentView_ = new QTextEdit(contentGroup);
    contentView_->setReadOnly(true);
    contentView_->setFont(QFont("Consolas", 9));
    contentView_->setMinimumHeight(120);
    contentLayout->addWidget(contentView_);
    layout->addWidget(contentGroup);

    auto* keysGroup = new QGroupBox("追踪项", this);
    auto* keysLayout = new QVBoxLayout(keysGroup);
    keyTree_ = new QTreeWidget(keysGroup);
    keyTree_->setHeaderLabels({"键/行", "追踪"});
    keyTree_->header()->setStretchLastSection(true);
    keyTree_->setRootIsDecorated(false);
    keyTree_->setMinimumHeight(80);
    toggleAllBtn_ = new QPushButton("全选/全不选", keysGroup);
    keysLayout->addWidget(keyTree_);
    keysLayout->addWidget(toggleAllBtn_);
    layout->addWidget(keysGroup);

    QObject::connect(toggleAllBtn_, &QPushButton::clicked, [this]() {
        bool allChecked = true;
        for (int i = 0; i < keyTree_->topLevelItemCount(); ++i)
            if (keyTree_->topLevelItem(i)->checkState(0) != Qt::Checked) allChecked = false;
        Qt::CheckState newState = allChecked ? Qt::Unchecked : Qt::Checked;
        for (int i = 0; i < keyTree_->topLevelItemCount(); ++i)
            keyTree_->topLevelItem(i)->setCheckState(0, newState);
    });
}

void ConfigEditor::setContent(const std::string& content)
{
    contentView_->setPlainText(QString::fromStdString(content));
}

void ConfigEditor::setSyncMode(const std::string& mode)
{
    int idx = modeCombo_->findText(QString::fromStdString(mode), Qt::MatchContains);
    if (idx >= 0) modeCombo_->setCurrentIndex(idx);
}

std::string ConfigEditor::syncMode() const
{
    QString t = modeCombo_->currentText();
    if (t.contains("full_sync")) return "full_sync";
    if (t.contains("config_merge")) return "config_merge";
    if (t.contains("line_by_line")) return "line_by_line";
    if (t.contains("no_sync")) return "no_sync";
    return "full_sync";
}

void ConfigEditor::setKeys(const QStringList& keys, const QStringList& tracked)
{
    keyTree_->clear();
    for (auto& k : keys) {
        auto* item = new QTreeWidgetItem(keyTree_);
        item->setText(0, k);
        item->setCheckState(0, tracked.contains(k) ? Qt::Checked : Qt::Unchecked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    }
}

QStringList ConfigEditor::trackedKeys() const
{
    QStringList result;
    for (int i = 0; i < keyTree_->topLevelItemCount(); ++i)
        if (keyTree_->topLevelItem(i)->checkState(0) == Qt::Checked)
            result << keyTree_->topLevelItem(i)->text(0);
    return result;
}

void ConfigEditor::setLineMode(bool lineMode)
{
    lineMode_ = lineMode;
    keyTree_->setHeaderLabels(lineMode_ ? QStringList{"行号", "追踪"} : QStringList{"键", "追踪"});
}

std::string ConfigEditor::mergePreview() const { return ""; }

void ConfigEditor::setTrackedLines(const QList<int>& lines)
{
    QSet<int> set(lines.begin(), lines.end());
    keyTree_->clear();
    QStringList contentLines = contentView_->toPlainText().split('\n');
    for (int i = 0; i < contentLines.size(); ++i) {
        auto* item = new QTreeWidgetItem(keyTree_);
        item->setText(0, QString::number(i + 1));
        item->setCheckState(0, set.contains(i + 1) ? Qt::Checked : Qt::Unchecked);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    }
}

QList<int> ConfigEditor::trackedLines() const
{
    QList<int> result;
    for (int i = 0; i < keyTree_->topLevelItemCount(); ++i)
        if (keyTree_->topLevelItem(i)->checkState(0) == Qt::Checked)
            result << keyTree_->topLevelItem(i)->text(0).toInt();
    return result;
}
