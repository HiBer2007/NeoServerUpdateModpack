#include "folder_policy_editor.h"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>

namespace GUIWorker {

namespace {

QString policyLabel(const QString& policy)
{
    if (policy == QLatin1String("skip")) {
        return QString::fromUtf8("\u8df3\u8fc7 (skip)\uff1a\u4e0d\u540c\u6b65\u8be5\u5939");
    }
    if (policy == QLatin1String("mirror")) {
        return QString::fromUtf8("\u955c\u50cf (mirror)\uff1a\u4e25\u683c\u8986\u76d6\uff0c\u6e05\u7a7a\u91cd\u5199\u5e76\u5220\u9664\u591a\u4f59\u9879");
    }
    if (policy == QLatin1String("incremental_overwrite")) {
        return QString::fromUtf8("\u589e\u91cf\u8986\u76d6 (incremental_overwrite)\uff1a\u4fdd\u7559\u591a\u4f59\u9879\uff0c\u4fee\u6539\u8fc7\u4e5f\u5199\u5165");
    }
    if (policy == QLatin1String("incremental_add")) {
        return QString::fromUtf8("\u589e\u91cf\u8865\u5145 (incremental_add)\uff1a\u53ea\u8865\u7f3a\u5931\uff0c\u5df2\u5b58\u5728\u7684\u4e0d\u52a8");
    }
    return QString::fromUtf8("\u9ed8\u8ba4\u8ddf\u968f");
}

} // namespace

FolderPolicyEditor::FolderPolicyEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\u6587\u4ef6\u5939\u540c\u6b65\u7b56\u7565"), this);
    title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #e8eaed;"));

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #ffd54f; font-size: 11px;"));

    lay->addWidget(title);
    lay->addWidget(pathLabel_);
    lay->addWidget(stateLabel_);

    auto* group = new QGroupBox(QString::fromUtf8("\u540c\u6b65\u8c03\u6574"), this);
    auto* groupLay = new QVBoxLayout(group);
    groupLay->setSpacing(6);

    inheritRb_ = new QRadioButton(policyLabel(QString()), group);
    skipRb_ = new QRadioButton(policyLabel(QStringLiteral("skip")), group);
    mirrorRb_ = new QRadioButton(policyLabel(QStringLiteral("mirror")), group);
    overwriteRb_ = new QRadioButton(policyLabel(QStringLiteral("incremental_overwrite")), group);
    addRb_ = new QRadioButton(policyLabel(QStringLiteral("incremental_add")), group);

    auto* groupBtn = new QButtonGroup(this);
    groupBtn->addButton(inheritRb_, 0);
    groupBtn->addButton(skipRb_, 1);
    groupBtn->addButton(mirrorRb_, 2);
    groupBtn->addButton(overwriteRb_, 3);
    groupBtn->addButton(addRb_, 4);

    groupLay->addWidget(inheritRb_);
    groupLay->addWidget(skipRb_);
    groupLay->addWidget(mirrorRb_);
    groupLay->addWidget(overwriteRb_);
    groupLay->addWidget(addRb_);
    lay->addWidget(group);

    auto* scopeBox = new QGroupBox(QString::fromUtf8("\u4fdd\u5b58\u8303\u56f4"), this);
    auto* scopeLay = new QVBoxLayout(scopeBox);
    topRb_ = new QRadioButton(QString::fromUtf8("\u9876\u5c42\u9ed8\u8ba4\uff08\u5168\u90e8\u5206\u652f\uff09"), scopeBox);
    branchRb_ = new QRadioButton(QString::fromUtf8("\u5f53\u524d\u5206\u652f\u8986\u76d6"), scopeBox);
    branchRb_->setChecked(true);
    scopeLay->addWidget(topRb_);
    scopeLay->addWidget(branchRb_);
    lay->addWidget(scopeBox);

    auto* btnRow = new QHBoxLayout;
    saveButton_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58"), this);
    saveButton_->setFixedWidth(88);
    btnRow->addStretch(1);
    btnRow->addWidget(saveButton_);
    lay->addLayout(btnRow);
    lay->addStretch(1);

    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        QString policy;
        if (skipRb_->isChecked()) policy = QStringLiteral("skip");
        else if (mirrorRb_->isChecked()) policy = QStringLiteral("mirror");
        else if (overwriteRb_->isChecked()) policy = QStringLiteral("incremental_overwrite");
        else if (addRb_->isChecked()) policy = QStringLiteral("incremental_add");
        emit saveRequested(folderPath_, policy, branchRb_->isChecked());
        emit contentModified();
    });

    applyStyle();
}

void FolderPolicyEditor::load(const QString& folderPath,
    const QString& effectivePolicy, bool branchOverrides,
    const QString& branchName)
{
    folderPath_ = folderPath;
    branchName_ = branchName;

    pathLabel_->setText(folderPath);
    stateLabel_->setText(branchOverrides
        ? QString::fromUtf8("\u2714 \u5f53\u524d\u5206\u652f\u5df2\u8986\u76d6\u9876\u5c42\u8bbe\u7f6e")
        : QString::fromUtf8("\u2718 \u7ee7\u627f\u9876\u5c42\u8bbe\u7f6e\uff08\u672a\u5728\u672c\u5206\u652f\u8986\u76d6\uff09"));

    if (effectivePolicy == QLatin1String("skip")) skipRb_->setChecked(true);
    else if (effectivePolicy == QLatin1String("mirror")) mirrorRb_->setChecked(true);
    else if (effectivePolicy == QLatin1String("incremental_overwrite")) overwriteRb_->setChecked(true);
    else if (effectivePolicy == QLatin1String("incremental_add")) addRb_->setChecked(true);
    else inheritRb_->setChecked(true);

    Q_UNUSED(branchName);
}

void FolderPolicyEditor::setScopeTop()
{
    topRb_->setChecked(true);
}

void FolderPolicyEditor::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QGroupBox {
            color: #c8ccd2;
            border: 1px solid #454b54;
            border-radius: 6px;
            margin-top: 8px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QRadioButton {
            color: #d8dce2;
            spacing: 6px;
        }
        QPushButton {
            background-color: #3a6ea5;
            color: #ffffff;
            border: none;
            border-radius: 4px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #4682b8;
        }
    )"));
}

} // namespace GUIWorker

#include "folder_policy_editor.moc"
