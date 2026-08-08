#include "sync_policies_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>

#include "sync_policy_display.h"

namespace GUIWorker {

QList<QPair<QString, QString>> folderPolicyDisplayItems()
{
    return {
        { QString::fromUtf8("\u8ddf\u968f\u9ed8\u8ba4 (default)\uff1a\u4f7f\u7528\u9ed8\u8ba4\u6587\u4ef6\u5939\u7b56\u7565"), QStringLiteral("default") },
        { QString::fromUtf8("\u589e\u91cf\u8865\u5145 (incremental_add)\uff1a\u53ea\u8865\u7f3a\u5931\uff0c\u5df2\u5b58\u5728\u7684\u4e0d\u52a8"), QStringLiteral("incremental_add") },
        { QString::fromUtf8("\u589e\u91cf\u8986\u76d6 (incremental_overwrite)\uff1a\u4fdd\u7559\u591a\u4f59\u9879\uff0c\u4fee\u6539\u8fc7\u4e5f\u5199\u5165"), QStringLiteral("incremental_overwrite") },
        { QString::fromUtf8("\u955c\u50cf (mirror)\uff1a\u4e25\u683c\u8986\u76d6\uff0c\u6e05\u7a7a\u91cd\u5199\u5e76\u5220\u9664\u591a\u4f59\u9879"), QStringLiteral("mirror") },
        { QString::fromUtf8("\u8df3\u8fc7 (skip)\uff1a\u4e0d\u5904\u7406\u6b64\u6587\u4ef6\u5939"), QStringLiteral("skip") },
    };
}

QList<QPair<QString, QString>> fileModeDisplayItems()
{
    return {
        { QString::fromUtf8("\u5168\u91cf\u8986\u76d6 (full)\uff1a\u76f4\u63a5\u5199\u5165\u76ee\u6807"), QStringLiteral("full") },
        { QString::fromUtf8("\u90e8\u5206\u540c\u6b65 (partial)\uff1a\u53ea\u66f4\u65b0\u8ffd\u8e2a\u7684\u952e/\u884c\uff0c\u4fdd\u7559\u76ee\u6807\u672c\u5730\u5185\u5bb9"), QStringLiteral("partial") },
        { QString::fromUtf8("\u5ffd\u7565 (ignore)\uff1a\u4e0d\u5199\u76ee\u6807"), QStringLiteral("ignore") },
    };
}

QList<QPair<QString, QString>> serverConfigModeDisplayItems()
{
    return {
        { QString::fromUtf8("\u8986\u76d6 (overwrite)\uff1a\u6574\u6587\u4ef6\u8986\u76d6"), QStringLiteral("overwrite") },
        { QString::fromUtf8("\u90e8\u5206\u540c\u6b65 (partial)\uff1a\u53ea\u66f4\u65b0\u8ffd\u8e2a\u7684\u952e/\u884c"), QStringLiteral("partial") },
        { QString::fromUtf8("\u5ffd\u7565 (ignore)\uff1a\u4e0d\u5199\u76ee\u6807"), QStringLiteral("ignore") },
    };
}

namespace {

int selectOrKeep(QComboBox* combo, const QString& value)
{
    int idx = combo->findData(value);
    if (idx < 0) {
        combo->addItem(
            QString::fromUtf8("\u672a\u77e5\u503c (%1)").arg(value), value);
        idx = combo->findData(value);
    }
    return idx;
}

QString joinList(const nlohmann::json& arr)
{
    if (!arr.is_array()) return QString();
    QStringList parts;
    for (const auto& v : arr) {
        if (v.is_string()) parts << QString::fromStdString(v.get<std::string>());
        else if (v.is_number_integer()) parts << QString::number(v.get<int>());
    }
    return parts.join(QStringLiteral(", "));
}

QStringList splitList(const QString& text)
{
    QStringList out;
    const auto raw = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const auto& s : raw) {
        const QString t = s.trimmed();
        if (!t.isEmpty()) out << t;
    }
    return out;
}

} // namespace

SyncPoliciesEditor::SyncPoliciesEditor(QWidget* parent)
    : QWidget(parent)
{
    buildUI();
}

void SyncPoliciesEditor::buildUI()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);

    auto* defRow = new QHBoxLayout();
    auto* defLabel = new QLabel(QString::fromUtf8("\u9ed8\u8ba4\u6587\u4ef6\u5939\u7b56\u7565:"), this);
    defaultPolicyCombo_ = new QComboBox(this);
    for (const auto& item : folderPolicyDisplayItems()) {
        defaultPolicyCombo_->addItem(item.first, item.second);
    }
    defaultPolicyCombo_->setMaxVisibleItems(8);
    defRow->addWidget(defLabel);
    defRow->addWidget(defaultPolicyCombo_, 1);
    lay->addLayout(defRow);

    auto* folderLabel = new QLabel(QString::fromUtf8("\u6587\u4ef6\u5939\u7b56\u7565 (folders)"), this);
    folderLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #8a9099; font-size: 11px;"));
    lay->addWidget(folderLabel);

    folderTable_ = new QTableWidget(this);
    folderTable_->setColumnCount(2);
    folderTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("\u76f8\u5bf9\u8def\u5f84"),
        QString::fromUtf8("\u7b56\u7565"),
    });
    folderTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    folderTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    folderTable_->verticalHeader()->setVisible(false);
    folderTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    folderTable_->setMinimumHeight(100);
    lay->addWidget(folderTable_);

    auto* folderBtnRow = new QHBoxLayout();
    addFolderBtn_ = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u6587\u4ef6\u5939..."), this);
    removeFolderBtn_ = new QPushButton(QString::fromUtf8("\u5220\u9664\u6240\u9009\u884c"), this);
    folderBtnRow->addWidget(addFolderBtn_);
    folderBtnRow->addWidget(removeFolderBtn_);
    folderBtnRow->addStretch(1);
    lay->addLayout(folderBtnRow);

    auto* fileLabel = new QLabel(QString::fromUtf8("\u6587\u4ef6\u7b56\u7565 (files)"), this);
    fileLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #8a9099; font-size: 11px;"));
    lay->addWidget(fileLabel);

    fileTable_ = new QTableWidget(this);
    fileTable_->setColumnCount(4);
    fileTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("\u76f8\u5bf9\u8def\u5f84"),
        QString::fromUtf8("\u6a21\u5f0f"),
        QString::fromUtf8("tracked_keys (\u9017\u53f7\u5206\u9694)"),
        QString::fromUtf8("tracked_lines (\u9017\u53f7\u5206\u9694)"),
    });
    fileTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    fileTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    fileTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    fileTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    fileTable_->verticalHeader()->setVisible(false);
    fileTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    fileTable_->setMinimumHeight(96);
    lay->addWidget(fileTable_);

    auto* fileBtnRow = new QHBoxLayout();
    addFileBtn_ = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u6587\u4ef6..."), this);
    removeFileBtn_ = new QPushButton(QString::fromUtf8("\u5220\u9664\u6240\u9009\u884c"), this);
    fileBtnRow->addWidget(addFileBtn_);
    fileBtnRow->addWidget(removeFileBtn_);
    fileBtnRow->addStretch(1);
    lay->addLayout(fileBtnRow);

    connect(addFolderBtn_, &QPushButton::clicked, this, &SyncPoliciesEditor::onAddFolderRow);
    connect(removeFolderBtn_, &QPushButton::clicked, this, &SyncPoliciesEditor::onRemoveFolderRow);
    connect(addFileBtn_, &QPushButton::clicked, this, &SyncPoliciesEditor::onAddFileRow);
    connect(removeFileBtn_, &QPushButton::clicked, this, &SyncPoliciesEditor::onRemoveFileRow);
    connect(defaultPolicyCombo_, QOverload<int>::of(&QComboBox::activated),
        this, &SyncPoliciesEditor::emitModified);
    connect(folderTable_, &QTableWidget::cellChanged,
        this, &SyncPoliciesEditor::emitModified);
    connect(fileTable_, &QTableWidget::cellChanged,
        this, &SyncPoliciesEditor::emitModified);
}

void SyncPoliciesEditor::load(const nlohmann::json& policies)
{
    original_ = policies.is_object() ? policies : nlohmann::json::object();
    folderTable_->setRowCount(0);
    fileTable_->setRowCount(0);

    defaultPolicyCombo_->blockSignals(true);
    const std::string def = policies.is_object()
        ? policies.value("default_folder_policy", std::string("incremental_add"))
        : std::string("incremental_add");
    defaultPolicyCombo_->setCurrentIndex(
        selectOrKeep(defaultPolicyCombo_, QString::fromStdString(def)));
    defaultPolicyCombo_->blockSignals(false);

    if (policies.is_object()) {
        if (policies.contains("folders") && policies["folders"].is_object()) {
            for (auto it = policies["folders"].begin(); it != policies["folders"].end(); ++it) {
                if (!it.value().is_string()) continue;
                const int row = folderTable_->rowCount();
                folderTable_->insertRow(row);
                folderTable_->setItem(row, 0,
                    new QTableWidgetItem(QString::fromStdString(it.key())));
                auto* combo = new QComboBox(folderTable_);
                for (const auto& item : folderPolicyDisplayItems()) {
                    combo->addItem(item.first, item.second);
                }
                combo->setMaxVisibleItems(8);
                const std::string v = it.value().get<std::string>();
                combo->setCurrentIndex(selectOrKeep(combo, QString::fromStdString(v)));
                connect(combo, QOverload<int>::of(&QComboBox::activated),
                    this, &SyncPoliciesEditor::emitModified);
                folderTable_->setCellWidget(row, 1, combo);
            }
        }
        if (policies.contains("files") && policies["files"].is_object()) {
            for (auto it = policies["files"].begin(); it != policies["files"].end(); ++it) {
                const auto& v = it.value();
                if (!v.is_object()) continue;
                const int row = fileTable_->rowCount();
                fileTable_->insertRow(row);
                fileTable_->setItem(row, 0,
                    new QTableWidgetItem(QString::fromStdString(it.key())));
                auto* combo = new QComboBox(fileTable_);
                for (const auto& item : fileModeDisplayItems()) {
                    combo->addItem(item.first, item.second);
                }
                combo->setMaxVisibleItems(8);
                const std::string mode = v.value("mode", std::string("full"));
                combo->setCurrentIndex(selectOrKeep(combo, QString::fromStdString(mode)));
                connect(combo, QOverload<int>::of(&QComboBox::activated),
                    this, &SyncPoliciesEditor::emitModified);
                fileTable_->setCellWidget(row, 1, combo);
                fileTable_->setItem(row, 2,
                    new QTableWidgetItem(joinList(v.value("tracked_keys", nlohmann::json::array()))));
                fileTable_->setItem(row, 3,
                    new QTableWidgetItem(joinList(v.value("tracked_lines", nlohmann::json::array()))));
            }
        }
    }
}

nlohmann::json SyncPoliciesEditor::save() const
{
    nlohmann::json folders = nlohmann::json::object();
    for (int r = 0; r < folderTable_->rowCount(); ++r) {
        auto* pathItem = folderTable_->item(r, 0);
        if (!pathItem) continue;
        const QString path = pathItem->text().trimmed();
        if (path.isEmpty()) continue;
        auto* combo = qobject_cast<QComboBox*>(folderTable_->cellWidget(r, 1));
        const std::string policy = combo
            ? combo->currentData().toString().toStdString()
            : std::string("incremental_add");
        folders[path.toStdString()] = policy;
    }

    nlohmann::json files = nlohmann::json::object();
    for (int r = 0; r < fileTable_->rowCount(); ++r) {
        auto* pathItem = fileTable_->item(r, 0);
        if (!pathItem) continue;
        const QString path = pathItem->text().trimmed();
        if (path.isEmpty()) continue;
        auto* combo = qobject_cast<QComboBox*>(fileTable_->cellWidget(r, 1));
        nlohmann::json entry;
        entry["mode"] = combo
            ? combo->currentData().toString().toStdString()
            : std::string("full");
        const QString keys = fileTable_->item(r, 2) ? fileTable_->item(r, 2)->text() : QString();
        const QString lines = fileTable_->item(r, 3) ? fileTable_->item(r, 3)->text() : QString();
        nlohmann::json keysArr = nlohmann::json::array();
        for (const auto& k : splitList(keys)) keysArr.push_back(k.toStdString());
        nlohmann::json linesArr = nlohmann::json::array();
        for (const auto& l : splitList(lines)) {
            bool ok = false;
            const int n = l.toInt(&ok);
            if (ok) linesArr.push_back(n);
        }
        if (!keysArr.empty()) entry["tracked_keys"] = keysArr;
        if (!linesArr.empty()) entry["tracked_lines"] = linesArr;
        files[path.toStdString()] = entry;
    }

    if (folders.empty() && files.empty()) {
        return nullptr;
    }

    nlohmann::json result = original_;
    result["default_folder_policy"] = defaultPolicyCombo_->currentData().toString().toStdString();
    result["folders"] = folders;
    result["files"] = files;
    return result;
}

void SyncPoliciesEditor::setFolderCandidates(const QStringList& dirs)
{
    setProperty("folderCandidates", dirs);
}

void SyncPoliciesEditor::onAddFolderRow()
{
    QString path;
    const QStringList candidates = property("folderCandidates").toStringList();
    if (!candidates.isEmpty()) {
        QDialog dlg(this);
        dlg.setWindowTitle(QString::fromUtf8("\u6dfb\u52a0\u6587\u4ef6\u5939\u7b56\u7565"));
        dlg.setMinimumWidth(340);
        auto* lay = new QVBoxLayout(&dlg);
        lay->addWidget(new QLabel(
            QString::fromUtf8("\u9009\u62e9\u5206\u652f\u6839\u76ee\u5f55 (\u53cc\u51fb\u786e\u5b9a)\uff0c\u6216\u81ea\u5b9a\u4e49:"), &dlg));
        auto* list = new QListWidget(&dlg);
        list->addItems(candidates);
        lay->addWidget(list, 1);
        auto* edit = new QLineEdit(&dlg);
        edit->setPlaceholderText(QString::fromUtf8("\u81ea\u5b9a\u4e49\u76f8\u5bf9\u8def\u5f84..."));
        lay->addWidget(edit);
        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        buttons->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("\u786e\u5b9a"));
        buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("\u53d6\u6d88"));
        lay->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        connect(list, &QListWidget::itemClicked, [&dlg, edit](QListWidgetItem* it) {
            edit->setText(it->text());
            dlg.activateWindow();
        });
        connect(list, &QListWidget::itemDoubleClicked, &dlg, &QDialog::accept);
        if (dlg.exec() != QDialog::Accepted) return;

        path = edit->text().trimmed();
        if (path.isEmpty() && list->currentItem()) {
            path = list->currentItem()->text().trimmed();
        }
        if (path.isEmpty()) return;

        for (int r = 0; r < folderTable_->rowCount(); ++r) {
            auto* item = folderTable_->item(r, 0);
            if (item && item->text().trimmed() == path) {
                folderTable_->setCurrentCell(r, 0);
                emitModified();
                return;
            }
        }
    }

    const int row = folderTable_->rowCount();
    folderTable_->insertRow(row);
    if (!path.isEmpty()) {
        folderTable_->setItem(row, 0, new QTableWidgetItem(path));
    } else {
        folderTable_->setItem(row, 0, new QTableWidgetItem());
    }
    auto* combo = new QComboBox(folderTable_);
    for (const auto& item : folderPolicyDisplayItems()) {
        combo->addItem(item.first, item.second);
    }
    combo->setMaxVisibleItems(8);
    connect(combo, QOverload<int>::of(&QComboBox::activated),
        this, &SyncPoliciesEditor::emitModified);
    folderTable_->setCellWidget(row, 1, combo);
    folderTable_->setCurrentCell(row, 0);
    folderTable_->editItem(folderTable_->item(row, 0));
    emitModified();
}

void SyncPoliciesEditor::onRemoveFolderRow()
{
    const int row = folderTable_->currentRow();
    if (row >= 0) {
        folderTable_->removeRow(row);
        emitModified();
    }
}

void SyncPoliciesEditor::onAddFileRow()
{
    const int row = fileTable_->rowCount();
    fileTable_->insertRow(row);
    fileTable_->setItem(row, 0, new QTableWidgetItem());
    auto* combo = new QComboBox(fileTable_);
    for (const auto& item : fileModeDisplayItems()) {
        combo->addItem(item.first, item.second);
    }
    combo->setMaxVisibleItems(8);
    connect(combo, QOverload<int>::of(&QComboBox::activated),
        this, &SyncPoliciesEditor::emitModified);
    fileTable_->setCellWidget(row, 1, combo);
    fileTable_->setItem(row, 2, new QTableWidgetItem());
    fileTable_->setItem(row, 3, new QTableWidgetItem());
    fileTable_->setCurrentCell(row, 0);
    fileTable_->editItem(fileTable_->item(row, 0));
    emitModified();
}

void SyncPoliciesEditor::onRemoveFileRow()
{
    const int row = fileTable_->currentRow();
    if (row >= 0) {
        fileTable_->removeRow(row);
        emitModified();
    }
}

void SyncPoliciesEditor::emitModified()
{
    emit contentModified();
}

SyncPoliciesDialog::SyncPoliciesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("\u7f16\u8f91\u540c\u6b65\u7b56\u7565"));
    setMinimumSize(560, 460);

    auto* lay = new QVBoxLayout(this);
    editor_ = new SyncPoliciesEditor(this);
    lay->addWidget(editor_, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QString::fromUtf8("\u786e\u5b9a"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QString::fromUtf8("\u53d6\u6d88"));
    lay->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SyncPoliciesDialog::load(const nlohmann::json& policies)
{
    editor_->load(policies);
}

nlohmann::json SyncPoliciesDialog::save() const
{
    return editor_->save();
}

} // namespace GUIWorker

#include "sync_policies_editor.moc"
