#include "branch_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QHeaderView>
#include <QScrollArea>
#include <QSplitter>
#include <QFont>

#include <algorithm>
#include <set>

#include <logger.h>

namespace GUIWorker {

BranchEditor::BranchEditor(QWidget* parent)
    : QWidget(parent), modified_(false), currentIndex_(-1)
{
    buildUI();
    connectSignals();
}

void BranchEditor::buildUI()
{
    auto* outerLayout = new QVBoxLayout(this);

    titleLabel_ = new QLabel("分支管理", this);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);

    auto* contentLayout = new QHBoxLayout();

    listGroup_ = new QGroupBox("分支列表", this);
    auto* listLayout = new QVBoxLayout(listGroup_);
    branchTable_ = new QTableWidget(0, 2, listGroup_);
    branchTable_->setHorizontalHeaderLabels({"分支名", "父分支"});
    branchTable_->horizontalHeader()->setStretchLastSection(true);
    branchTable_->verticalHeader()->setVisible(false);
    branchTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    branchTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    auto* listBtnLayout = new QHBoxLayout();
    addBranchBtn_ = new QPushButton("添加分支", listGroup_);
    removeBranchBtn_ = new QPushButton("删除分支", listGroup_);
    listBtnLayout->addWidget(addBranchBtn_);
    listBtnLayout->addWidget(removeBranchBtn_);
    listBtnLayout->addStretch();
    listLayout->addWidget(branchTable_);
    listLayout->addLayout(listBtnLayout);

    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    editorGroup_ = new QGroupBox("分支属性", rightPanel);
    auto* editorLayout = new QFormLayout(editorGroup_);
    nameEdit_ = new QLineEdit(editorGroup_);
    nameEdit_->setPlaceholderText("分支名称 (英文)");
    parentCombo_ = new QComboBox(editorGroup_);
    parentCombo_->addItem("(无 - 根分支)", QVariant(QString("")));
    modloaderVersionEdit_ = new QLineEdit(editorGroup_);
    modloaderVersionEdit_->setPlaceholderText("例如: 0.16.10 (留空则继承全局)");
    descriptionEdit_ = new QLineEdit(editorGroup_);
    descriptionEdit_->setPlaceholderText("分支说明, 例如: 纯净生存玩法服 (可选)");
    hiddenCheck_ = new QCheckBox("隐藏此分支 (不在引导GUI中展示, 仍可通过命令行指定)", editorGroup_);
    editorLayout->addRow("分支名:", nameEdit_);
    editorLayout->addRow("父分支:", parentCombo_);
    editorLayout->addRow("加载器版本:", modloaderVersionEdit_);
    editorLayout->addRow("描述:", descriptionEdit_);
    editorLayout->addRow("", hiddenCheck_);

    syncPoliciesGroup_ = new QGroupBox("同步策略 (分支级覆盖)", rightPanel);
    auto* spLayout = new QVBoxLayout(syncPoliciesGroup_);
    spInfoLabel_ = new QLabel(syncPoliciesGroup_);
    spInfoLabel_->setStyleSheet("color: gray; font-size: 11px;");
    spInfoLabel_->setWordWrap(true);
    spInfoLabel_->setTextFormat(Qt::PlainText);
    editPoliciesBtn_ = new QPushButton("编辑分支同步策略...", syncPoliciesGroup_);
    policiesSummary_ = new QLabel(syncPoliciesGroup_);
    policiesSummary_->setStyleSheet("color: gray; font-size: 11px;");
    policiesSummary_->setWordWrap(true);
    policiesSummary_->setTextFormat(Qt::PlainText);
    spLayout->addWidget(spInfoLabel_);
    spLayout->addWidget(editPoliciesBtn_);
    spLayout->addWidget(policiesSummary_);

    inheritanceGroup_ = new QGroupBox("继承链", rightPanel);
    auto* inLayout = new QVBoxLayout(inheritanceGroup_);
    inheritanceTree_ = new QTreeWidget(inheritanceGroup_);
    inheritanceTree_->setHeaderLabels({"层级", "分支"});
    inheritanceTree_->setRootIsDecorated(true);
    inLayout->addWidget(inheritanceTree_);

    warningLabel_ = new QLabel("", rightPanel);
    warningLabel_->setStyleSheet("color: orange;");
    warningLabel_->setWordWrap(true);

    rightLayout->addWidget(editorGroup_);
    rightLayout->addWidget(syncPoliciesGroup_);
    rightLayout->addWidget(inheritanceGroup_, 1);
    rightLayout->addWidget(warningLabel_);
    rightLayout->addStretch();

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(listGroup_);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    saveBtn_ = new QPushButton("保存分支配置", this);
    saveBtn_->setMinimumHeight(36);

    titleLabel_->setFixedHeight(titleLabel_->sizeHint().height());
    outerLayout->addWidget(titleLabel_);
    outerLayout->addWidget(splitter, 1);
    outerLayout->addWidget(saveBtn_);
}

void BranchEditor::connectSignals()
{
    connect(addBranchBtn_, &QPushButton::clicked, this, &BranchEditor::onAddBranch);
    connect(removeBranchBtn_, &QPushButton::clicked, this, &BranchEditor::onRemoveBranch);
    connect(branchTable_, &QTableWidget::currentCellChanged,
        [this](int row, int, int, int) {
            if (row >= 0 && row < static_cast<int>(branches_.size())) {
                currentIndex_ = row;
                onBranchSelected();
            }
        });

    connect(nameEdit_, &QLineEdit::textChanged, this, &BranchEditor::onNameChanged);
    connect(parentCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &BranchEditor::onParentChanged);
    connect(modloaderVersionEdit_, &QLineEdit::textChanged,
        this, &BranchEditor::onModloaderVersionChanged);
    connect(descriptionEdit_, &QLineEdit::textChanged,
        this, &BranchEditor::onDescriptionChanged);
    connect(hiddenCheck_, &QCheckBox::toggled,
        this, &BranchEditor::onHiddenToggled);
    connect(editPoliciesBtn_, &QPushButton::clicked,
        this, &BranchEditor::onEditSyncPolicies);

    connect(saveBtn_, &QPushButton::clicked, [this]() {
        syncCurrentBranchToTable();
        if (currentIndex_ < 0) return;

        std::vector<nlohmann::json> result;
        for (auto& b : branches_) {
            nlohmann::json j = b.raw.is_null() ? nlohmann::json::object() : b.raw;
            j["name"] = b.name;
            j["parent"] = b.parent.empty() ? nullptr : nlohmann::json(b.parent);
            j["modloader_version"] = b.modloaderVersion;
            j["description"] = b.description;
            j["hidden"] = b.hidden;
            result.push_back(j);
        }
        emit saveRequested(QString::fromStdString(nlohmann::json(result).dump()));
        modified_ = false;
        CLogger::Info("BranchEditor: branch config saved ({} branches)", branches_.size());
    });
}

void BranchEditor::loadBranches(
    const std::vector<nlohmann::json>& branches,
    const std::string& defaultBranch)
{
    branches_.clear();
    for (auto& b : branches) {
        BranchRow row;
        row.name = b.value("name", "");
        if (b.contains("parent") && b["parent"].is_string())
            row.parent = b["parent"].get<std::string>();
        row.modloaderVersion = b.value("modloader_version", "");
        row.description = b.value("description", "");
        row.hidden = b.value("hidden", false);
        row.raw = b;
        branches_.push_back(row);
    }

    refreshParentCombos();

    branchTable_->setRowCount(0);
    for (size_t i = 0; i < branches_.size(); ++i) {
        auto& b = branches_[i];
        branchTable_->insertRow(static_cast<int>(i));
        branchTable_->setItem(static_cast<int>(i), 0,
            new QTableWidgetItem(QString::fromStdString(b.name)));
        branchTable_->setItem(static_cast<int>(i), 1,
            new QTableWidgetItem(QString::fromStdString(b.parent)));
    }

    if (!branches_.empty()) {
        branchTable_->selectRow(0);
        currentIndex_ = 0;
        onBranchSelected();
    }

    modified_ = false;
}

std::vector<nlohmann::json> BranchEditor::saveBranches() const
{
    std::vector<nlohmann::json> result;
    for (auto& b : branches_) {
        nlohmann::json j = b.raw.is_null() ? nlohmann::json::object() : b.raw;
        j["name"] = b.name;
        if (b.parent.empty()) {
            j["parent"] = nullptr;
        } else {
            j["parent"] = b.parent;
        }
        j["modloader_version"] = b.modloaderVersion;
        j["description"] = b.description;
        j["hidden"] = b.hidden;
        result.push_back(j);
    }
    return result;
}

bool BranchEditor::isModified() const { return modified_; }

void BranchEditor::onAddBranch()
{
    BranchRow row;
    row.name = "new-branch-" + std::to_string(branches_.size());
    branches_.push_back(row);

    int idx = static_cast<int>(branches_.size()) - 1;
    branchTable_->insertRow(idx);
    branchTable_->setItem(idx, 0, new QTableWidgetItem(QString::fromStdString(row.name)));
    branchTable_->setItem(idx, 1, new QTableWidgetItem(""));

    refreshParentCombos();
    branchTable_->selectRow(idx);
    currentIndex_ = idx;
    onBranchSelected();

    nameEdit_->setFocus();
    nameEdit_->selectAll();
    markModified();
}

void BranchEditor::onRemoveBranch()
{
    int row = branchTable_->currentRow();
    if (row < 0 || row >= static_cast<int>(branches_.size())) return;

    std::string removedName = branches_[row].name;

    bool hasChildren = false;
    for (auto& b : branches_) {
        if (b.parent == removedName) hasChildren = true;
    }
    if (hasChildren) {
        QMessageBox::warning(this, "无法删除",
            QString("分支 \"%1\" 有子分支，请先删除或重新指定子分支的父分支。")
                .arg(QString::fromStdString(removedName)));
        return;
    }

    if (branches_.size() <= 1) {
        QMessageBox::warning(this, "无法删除", "至少需要保留一个分支。");
        return;
    }

    branches_.erase(branches_.begin() + row);
    branchTable_->removeRow(row);
    refreshParentCombos();

    if (!branches_.empty()) {
        if (row >= static_cast<int>(branches_.size())) row = static_cast<int>(branches_.size()) - 1;
        branchTable_->selectRow(row);
        currentIndex_ = row;
        onBranchSelected();
    }

    rebuildInheritanceTree();
    markModified();
}

void BranchEditor::onBranchSelected()
{
    if (currentIndex_ < 0 || currentIndex_ >= static_cast<int>(branches_.size())) return;

    syncTableToCurrentBranch();
    rebuildInheritanceTree();
    updatePoliciesSummary();

    spInfoLabel_->setText(
        QString("分支级 sync_policies 覆盖 (留空继承顶层默认)。"
            "serverconfig 规则: branches/%1/[save]/serverconfig/.rule/ "
            "(globle.json 默认模式 / list.json 文件清单)，在仓库树选中 "
            "[save]/serverconfig 文件夹可编辑。")
            .arg(QString::fromStdString(branches_[currentIndex_].name)));
}

void BranchEditor::onEditSyncPolicies()
{
    if (currentIndex_ < 0) return;
    auto& cur = branches_[currentIndex_];

    nlohmann::json current;
    if (cur.raw.is_object() && cur.raw.contains("sync_policies")
        && !cur.raw["sync_policies"].is_null()) {
        current = cur.raw["sync_policies"];
    } else {
        current = nullptr;
    }

    SyncPoliciesDialog dlg(this);
    dlg.load(current);
    if (dlg.exec() != QDialog::Accepted) return;

    nlohmann::json sp = dlg.save();
    if (cur.raw.is_null()) {
        cur.raw = nlohmann::json::object();
    }
    if (sp.is_null()) {
        cur.raw.erase("sync_policies");
    } else {
        cur.raw["sync_policies"] = std::move(sp);
    }

    updatePoliciesSummary();
    markModified();
    CLogger::Info("BranchEditor: branch sync config updated: {}",
        branches_[currentIndex_].name);
}

void BranchEditor::updatePoliciesSummary()
{
    if (currentIndex_ < 0) return;
    const auto& raw = branches_[currentIndex_].raw;
    if (!raw.is_object() || !raw.contains("sync_policies")
        || raw["sync_policies"].is_null()) {
        policiesSummary_->setText("未设置 (继承顶层默认策略)");
        return;
    }
    const auto& sp = raw["sync_policies"];
    const int folders = sp.contains("folders") && sp["folders"].is_object()
        ? static_cast<int>(sp["folders"].size()) : 0;
    const int files = sp.contains("files") && sp["files"].is_object()
        ? static_cast<int>(sp["files"].size()) : 0;
    policiesSummary_->setText(
        QString("当前分支已设置: %1 个文件夹策略, %2 个文件策略")
            .arg(folders).arg(files));
}

void BranchEditor::onNameChanged(const QString& text)
{
    if (currentIndex_ < 0) return;
    branches_[currentIndex_].name = text.toStdString();

    branchTable_->item(currentIndex_, 0)->setText(text);
    refreshParentCombos();
    rebuildInheritanceTree();
    markModified();
}

void BranchEditor::onParentChanged(int index)
{
    if (currentIndex_ < 0) return;
    branches_[currentIndex_].parent = parentCombo_->itemData(index).toString().toStdString();

    branchTable_->item(currentIndex_, 1)->setText(
        QString::fromStdString(branches_[currentIndex_].parent));

    validateBranchConfig();
    rebuildInheritanceTree();
    markModified();
}

void BranchEditor::onModloaderVersionChanged(const QString& text)
{
    if (currentIndex_ < 0) return;
    branches_[currentIndex_].modloaderVersion = text.toStdString();
    markModified();
}

void BranchEditor::onDescriptionChanged(const QString& text)
{
    if (currentIndex_ < 0) return;
    branches_[currentIndex_].description = text.toStdString();
    markModified();
}

void BranchEditor::onHiddenToggled(bool checked)
{
    if (currentIndex_ < 0) return;
    branches_[currentIndex_].hidden = checked;
    markModified();
}

void BranchEditor::refreshParentCombos()
{
    parentCombo_->blockSignals(true);

    QString current = parentCombo_->currentData().toString();
    parentCombo_->clear();
    parentCombo_->addItem("(无 - 根分支)", QVariant(QString("")));

    for (size_t i = 0; i < branches_.size(); ++i) {
        parentCombo_->addItem(
            QString::fromStdString(branches_[i].name),
            QVariant(QString::fromStdString(branches_[i].name)));
    }

    int idx = parentCombo_->findData(QVariant(current));
    if (idx >= 0) parentCombo_->setCurrentIndex(idx);

    parentCombo_->blockSignals(false);
}

void BranchEditor::rebuildInheritanceTree()
{
    inheritanceTree_->clear();
    if (currentIndex_ < 0) return;

    std::string currentName = branches_[currentIndex_].name;

    std::set<std::string> visited;
    std::vector<std::string> chain;
    std::string name = currentName;

    for (int safety = 0; safety < 50; ++safety) {
        if (visited.count(name)) break;
        visited.insert(name);
        chain.insert(chain.begin(), name);

        bool found = false;
        for (auto& b : branches_) {
            if (b.name == name && !b.parent.empty()) {
                name = b.parent;
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    for (size_t i = 0; i < chain.size(); ++i) {
        auto* item = new QTreeWidgetItem(inheritanceTree_);
        item->setText(0, QString::number(i));
        item->setText(1, QString::fromStdString(chain[i]));
        if (chain[i] == currentName) {
            QFont boldFont = item->font(1);
            boldFont.setBold(true);
            item->setFont(1, boldFont);
        }
    }

    inheritanceTree_->expandAll();
}

void BranchEditor::syncTableToCurrentBranch()
{
    auto& b = branches_[currentIndex_];

    nameEdit_->blockSignals(true);
    parentCombo_->blockSignals(true);
    modloaderVersionEdit_->blockSignals(true);
    descriptionEdit_->blockSignals(true);
    hiddenCheck_->blockSignals(true);

    nameEdit_->setText(QString::fromStdString(b.name));
    int parentIdx = parentCombo_->findData(QVariant(QString::fromStdString(b.parent)));
    parentCombo_->setCurrentIndex(parentIdx >= 0 ? parentIdx : 0);
    modloaderVersionEdit_->setText(QString::fromStdString(b.modloaderVersion));
    descriptionEdit_->setText(QString::fromStdString(b.description));
    hiddenCheck_->setChecked(b.hidden);

    nameEdit_->blockSignals(false);
    parentCombo_->blockSignals(false);
    modloaderVersionEdit_->blockSignals(false);
    descriptionEdit_->blockSignals(false);
    hiddenCheck_->blockSignals(false);
}

void BranchEditor::syncCurrentBranchToTable()
{
    if (currentIndex_ < 0) return;
    auto& b = branches_[currentIndex_];
    b.name = nameEdit_->text().toStdString();
    b.parent = parentCombo_->currentData().toString().toStdString();
    b.modloaderVersion = modloaderVersionEdit_->text().toStdString();
    b.description = descriptionEdit_->text().toStdString();
    b.hidden = hiddenCheck_->isChecked();

    if (currentIndex_ < branchTable_->rowCount()) {
        branchTable_->item(currentIndex_, 0)->setText(QString::fromStdString(b.name));
        branchTable_->item(currentIndex_, 1)->setText(QString::fromStdString(b.parent));
    }
}

void BranchEditor::validateBranchConfig()
{
    syncCurrentBranchToTable();

    std::string selfName = (currentIndex_ >= 0) ? branches_[currentIndex_].name : "";
    std::string parentName = (currentIndex_ >= 0) ? branches_[currentIndex_].parent : "";

    if (parentName == selfName) {
        warningLabel_->setText("错误: 分支不能以自身为父分支。请重新选择。");
        return;
    }

    std::set<std::string> visited;
    std::string current = parentName;
    while (!current.empty()) {
        if (current == selfName) {
            warningLabel_->setText("错误: 检测到循环继承！请重新选择父分支。");
            return;
        }
        if (visited.count(current)) break;
        visited.insert(current);

        bool found = false;
        for (auto& b : branches_) {
            if (b.name == current) {
                current = b.parent;
                found = true;
                break;
            }
        }
        if (!found) break;
    }

    warningLabel_->setText("");
}

void BranchEditor::markModified()
{
    if (!modified_) {
        modified_ = true;
        emit contentModified();
    }
}

} // namespace GUIWorker

#include "branch_editor.moc"




