#include "pointer_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <logger.h>

namespace GUIWorker {

const QStringList PointerEditor::RESOLVER_TYPES = {
    "modrinth",
    "direct_url",
    "curseforge",
    "github_release"
};

PointerEditor::PointerEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* idGroup = new QGroupBox("指针标识", this);
    auto* idLayout = new QFormLayout(idGroup);

    sha256Edit_ = new QLineEdit(this);
    sha256Edit_->setReadOnly(true);
    sha256Edit_->setPlaceholderText("SHA-256 (自动从文件名读取)");
    sha256Edit_->setFont(QFont("Consolas", 10));
    sha256Edit_->setStyleSheet(
        "QLineEdit { background: #f0f0f0; color: #555; border: 1px solid #ccc; "
        "border-radius: 3px; padding: 4px; }");
    idLayout->addRow("SHA-256:", sha256Edit_);

    resolverCombo_ = new QComboBox(this);
    resolverCombo_->addItems(RESOLVER_TYPES);
    resolverCombo_->setCurrentIndex(0);
    idLayout->addRow("解析器类型:", resolverCombo_);

    layout->addWidget(idGroup);

    layout->addSpacing(8);

    auto* metaGroup = new QGroupBox("元数据", this);
    auto* metaLayout = new QVBoxLayout(metaGroup);

    metadataTable_ = new QTableWidget(0, 2, this);
    metadataTable_->setHorizontalHeaderLabels({"键", "值"});
    metadataTable_->horizontalHeader()->setStretchLastSection(true);
    metadataTable_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Interactive);
    metadataTable_->setAlternatingRowColors(true);
    metadataTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    metadataTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    metaLayout->addWidget(metadataTable_);

    auto* btnLayout = new QHBoxLayout();
    addRowBtn_ = new QPushButton("添加行", this);
    removeRowBtn_ = new QPushButton("删除选中行", this);
    btnLayout->addWidget(addRowBtn_);
    btnLayout->addWidget(removeRowBtn_);
    btnLayout->addStretch();
    metaLayout->addLayout(btnLayout);

    layout->addWidget(metaGroup);

    layout->addStretch();

    connect(addRowBtn_, &QPushButton::clicked,
        this, &PointerEditor::onAddRow);
    connect(removeRowBtn_, &QPushButton::clicked,
        this, &PointerEditor::onRemoveRow);
    connect(resolverCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PointerEditor::onResolverChanged);
    connect(metadataTable_, &QTableWidget::cellChanged,
        this, &PointerEditor::onCellChanged);
}

void PointerEditor::loadPointer(const NeoCore::PointerInfo& ptr)
{
    sha256Edit_->setText(QString::fromStdString(ptr.sha256));

    int resolverIdx = RESOLVER_TYPES.indexOf(
        QString::fromStdString(ptr.resolver));
    if (resolverIdx >= 0) {
        resolverCombo_->setCurrentIndex(resolverIdx);
    } else {
        resolverCombo_->setCurrentIndex(0);
    }

    metadataTable_->blockSignals(true);
    metadataTable_->setRowCount(0);

    if (ptr.metadata.is_object()) {
        for (auto it = ptr.metadata.begin(); it != ptr.metadata.end(); ++it) {
            int row = metadataTable_->rowCount();
            metadataTable_->insertRow(row);

            auto* keyItem = new QTableWidgetItem(
                QString::fromStdString(it.key()));
            metadataTable_->setItem(row, 0, keyItem);

            QString valStr;
            if (it.value().is_string()) {
                valStr = QString::fromStdString(it.value().get<std::string>());
            } else if (it.value().is_number()) {
                valStr = QString::number(it.value().get<double>());
            } else if (it.value().is_boolean()) {
                valStr = it.value().get<bool>() ? "true" : "false";
            } else {
                valStr = QString::fromStdString(it.value().dump());
            }

            auto* valItem = new QTableWidgetItem(valStr);
            metadataTable_->setItem(row, 1, valItem);
        }
    }

    metadataTable_->blockSignals(false);

    CLogger::Debug("PointerEditor: pointer loaded {}", ptr.sha256);
}

NeoCore::PointerInfo PointerEditor::pointerInfo() const
{
    NeoCore::PointerInfo info;
    info.sha256 = sha256Edit_->text().toStdString();
    info.resolver = resolverCombo_->currentText().toStdString();

    nlohmann::json meta = nlohmann::json::object();
    for (int row = 0; row < metadataTable_->rowCount(); ++row) {
        auto* keyItem = metadataTable_->item(row, 0);
        auto* valItem = metadataTable_->item(row, 1);

        if (!keyItem || keyItem->text().trimmed().isEmpty()) continue;

        QString key = keyItem->text().trimmed();
        QString val = valItem ? valItem->text().trimmed() : "";

        if (val == "true") {
            meta[key.toStdString()] = true;
        } else if (val == "false") {
            meta[key.toStdString()] = false;
        } else {
            bool isInt = false;
            val.toLongLong(&isInt);
            if (isInt) {
                meta[key.toStdString()] = val.toLongLong();
            } else {
                bool isDouble = false;
                val.toDouble(&isDouble);
                if (isDouble) {
                    meta[key.toStdString()] = val.toDouble();
                } else {
                    meta[key.toStdString()] = val.toStdString();
                }
            }
        }
    }
    info.metadata = meta;

    return info;
}

void PointerEditor::clear()
{
    sha256Edit_->clear();
    resolverCombo_->setCurrentIndex(0);
    metadataTable_->setRowCount(0);
}

void PointerEditor::onAddRow()
{
    int row = metadataTable_->rowCount();
    metadataTable_->insertRow(row);
    metadataTable_->setItem(row, 0, new QTableWidgetItem(""));
    metadataTable_->setItem(row, 1, new QTableWidgetItem(""));
    metadataTable_->editItem(metadataTable_->item(row, 0));
}

void PointerEditor::onRemoveRow()
{
    auto selected = metadataTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        int lastRow = metadataTable_->rowCount() - 1;
        if (lastRow >= 0) {
            metadataTable_->removeRow(lastRow);
        }
        return;
    }
    for (const auto& index : selected) {
        metadataTable_->removeRow(index.row());
    }
}

void PointerEditor::onResolverChanged(int index)
{
    Q_UNUSED(index);
    emit pointerModified();
}

void PointerEditor::onCellChanged(int row, int column)
{
    Q_UNUSED(row);
    Q_UNUSED(column);
    emit pointerModified();
}

} // namespace GUIWorker

#include "pointer_editor.moc"



