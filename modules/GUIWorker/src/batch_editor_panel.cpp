#include "batch_editor_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QRegularExpression>

namespace GUIWorker {

using HiBerGUI::RepoObjectInfo;
using HiBerGUI::RepoObjectType;

BatchEditorPanel::BatchEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    titleLabel_ = new QLabel(QString::fromUtf8("\u6279\u91cf\u7f16\u8f91"), this);
    titleLabel_->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #e8eaed;"));
    lay->addWidget(titleLabel_);

    auto* selLabel = new QLabel(
        QString::fromUtf8("\u9009\u4e2d\u9879:"), this);
    selLabel->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    lay->addWidget(selLabel);

    selectionList_ = new QListWidget(this);
    selectionList_->setAlternatingRowColors(true);
    selectionList_->setMaximumHeight(160);
    lay->addWidget(selectionList_);

    // ---- 通用区: 批量修改文件同步策略 ----
    policyGroup_ = new QGroupBox(
        QString::fromUtf8("\u6279\u91cf\u4fee\u6539\u540c\u6b65\u7b56\u7565"), this);
    auto* pLay = new QVBoxLayout(policyGroup_);
    pLay->setSpacing(6);

    auto* modeRow = new QHBoxLayout;
    auto* modeLabel = new QLabel(
        QString::fromUtf8("\u540c\u6b65\u6a21\u5f0f:"), policyGroup_);
    modeCombo_ = new QComboBox(policyGroup_);
    modeCombo_->addItem(QString::fromUtf8("\u8ddf\u968f\u6587\u4ef6\u5939"), "full");
    modeCombo_->addItem(QString::fromUtf8("\u5f3a\u5236\u8986\u76d6"), "force");
    modeCombo_->addItem(QString::fromUtf8("\u90e8\u5206\u540c\u6b65"), "partial");
    modeCombo_->addItem(QString::fromUtf8("\u5ffd\u7565"), "ignore");
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(modeCombo_, 1);
    pLay->addLayout(modeRow);

    auto* keysLabel = new QLabel(
        QString::fromUtf8("\u8ffd\u8e2a\u7684\u952e (\u6bcf\u884c\u4e00\u4e2a):"),
        policyGroup_);
    keysLabel->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    pLay->addWidget(keysLabel);
    keysEdit_ = new QPlainTextEdit(policyGroup_);
    keysEdit_->setMaximumHeight(100);
    pLay->addWidget(keysEdit_);

    auto* linesRow = new QHBoxLayout;
    auto* linesLabel = new QLabel(
        QString::fromUtf8("\u8ffd\u8e2a\u7684\u884c\u53f7:"), policyGroup_);
    linesEdit_ = new QLineEdit(policyGroup_);
    linesEdit_->setPlaceholderText(QStringLiteral("1,5,9"));
    linesRow->addWidget(linesLabel);
    linesRow->addWidget(linesEdit_, 1);
    pLay->addLayout(linesRow);

    auto* scopeRow = new QHBoxLayout;
    topRb_ = new QRadioButton(
        QString::fromUtf8("\u9876\u5c42\u9ed8\u8ba4"), policyGroup_);
    branchRb_ = new QRadioButton(
        QString::fromUtf8("\u5f53\u524d\u5206\u652f"), policyGroup_);
    branchRb_->setChecked(true);
    scopeRow->addWidget(topRb_);
    scopeRow->addWidget(branchRb_);
    scopeRow->addStretch(1);
    pLay->addLayout(scopeRow);

    applyButton_ = new QPushButton(
        QString::fromUtf8("\u5e94\u7528"), policyGroup_);
    pLay->addWidget(applyButton_);
    lay->addWidget(policyGroup_);

    connect(modeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, [this](int) { updatePartialEnabled(); });

    // ---- 专属区: 批量转 Modrinth 指针 (全 JAR 选集) ----
    jarGroup_ = new QGroupBox(
        QString::fromUtf8("JAR \u8f6c\u6362"), this);
    auto* jLay = new QVBoxLayout(jarGroup_);
    convertButton_ = new QPushButton(
        QString::fromUtf8("\u8f6c Modrinth \u6307\u9488"), jarGroup_);
    jLay->addWidget(convertButton_);
    lay->addWidget(jarGroup_);

    // ---- 专属区: 批量转回原始文件 (全指针选集) ----
    pointerGroup_ = new QGroupBox(
        QString::fromUtf8("\u6307\u9488\u8f6c\u56de"), this);
    auto* ptLay = new QVBoxLayout(pointerGroup_);
    restoreButton_ = new QPushButton(
        QString::fromUtf8("\u8f6c\u56de\u539f\u59cb\u6587\u4ef6"), pointerGroup_);
    ptLay->addWidget(restoreButton_);
    lay->addWidget(pointerGroup_);

    deleteButton_ = new QPushButton(
        QString::fromUtf8("\u5220\u9664\u9009\u4e2d"), this);
    lay->addWidget(deleteButton_);

    lay->addStretch(1);

    connect(applyButton_, &QPushButton::clicked, this,
        &BatchEditorPanel::applyPolicy);
    connect(convertButton_, &QPushButton::clicked, this, [this]() {
        QStringList paths;
        for (const RepoObjectInfo& info : selection_) {
            if (info.type == RepoObjectType::PlainFile) {
                paths << info.path;
            }
        }
        if (!paths.isEmpty()) {
            emit batchConvertJarsRequested(paths);
        }
    });
    connect(restoreButton_, &QPushButton::clicked, this, [this]() {
        QList<RepoObjectInfo> pointers;
        for (const RepoObjectInfo& info : selection_) {
            if (info.type == RepoObjectType::Pointer) {
                pointers << info;
            }
        }
        if (!pointers.isEmpty()) {
            emit batchRestorePointersRequested(pointers);
        }
    });
    connect(deleteButton_, &QPushButton::clicked, this, [this]() {
        if (!selection_.isEmpty()) {
            emit batchDeleteRequested(selection_);
        }
    });

    updateSections();
    updatePartialEnabled();
    applyStyle();
}

void BatchEditorPanel::loadSelection(const QList<RepoObjectInfo>& infos)
{
    selection_ = infos;
    selectionList_->clear();
    for (const RepoObjectInfo& info : infos) {
        QString kind;
        if (info.type == RepoObjectType::Pointer) {
            kind = QString::fromUtf8("\u6307\u9488");
        } else if (info.type == RepoObjectType::PlainFile
            && info.path.endsWith(QStringLiteral(".jar"),
                Qt::CaseInsensitive)) {
            kind = QStringLiteral("JAR");
        } else if (info.type == RepoObjectType::ConfigFile) {
            kind = QString::fromUtf8("\u914d\u7f6e");
        } else if (info.type == RepoObjectType::Folder) {
            kind = QString::fromUtf8("\u6587\u4ef6\u5939");
        } else {
            kind = QString::fromUtf8("\u6587\u4ef6");
        }
        selectionList_->addItem(kind + QStringLiteral("  ") + info.path);
    }
    titleLabel_->setText(QString::fromUtf8("\u6279\u91cf\u7f16\u8f91: \u9009\u4e2d %1 \u9879")
        .arg(infos.size()));
    updateSections();
}

void BatchEditorPanel::clearSelection()
{
    selection_.clear();
    selectionList_->clear();
    titleLabel_->setText(QString::fromUtf8("\u6279\u91cf\u7f16\u8f91"));
    updateSections();
}

void BatchEditorPanel::setHasParser(bool hasParser)
{
    hasParser_ = hasParser;
    updatePartialEnabled();
}

void BatchEditorPanel::applyPolicy()
{
    if (selection_.isEmpty()) return;
    QStringList paths;
    for (const RepoObjectInfo& info : selection_) {
        if (info.type == RepoObjectType::Folder) continue;
        paths << info.path;
    }
    if (paths.isEmpty()) return;

    const QString mode = modeCombo_->currentData().toString();
    std::vector<std::string> keys;
    const QStringList keyLines = keysEdit_->toPlainText().split(
        QRegularExpression(QStringLiteral("[\\n\\r,;]+")),
        Qt::SkipEmptyParts);
    for (const QString& k : keyLines) {
        keys.push_back(k.trimmed().toStdString());
    }
    std::vector<int> lines;
    const QStringList lineParts = linesEdit_->text().split(
        QRegularExpression(QStringLiteral("[\\s,;]+")),
        Qt::SkipEmptyParts);
    for (const QString& p : lineParts) {
        bool ok = false;
        const int v = p.toInt(&ok);
        if (ok && v > 0) lines.push_back(v);
    }
    emit batchPolicySaveRequested(paths, mode, keys, lines,
        branchRb_->isChecked());
    emit contentModified();
}

void BatchEditorPanel::updateSections()
{
    bool anyFolder = false;
    bool allJars = !selection_.isEmpty();
    bool allPointers = !selection_.isEmpty();
    for (const RepoObjectInfo& info : selection_) {
        const bool isJar = info.type == RepoObjectType::PlainFile
            && info.path.endsWith(QStringLiteral(".jar"),
                Qt::CaseInsensitive);
        if (info.type == RepoObjectType::Folder) anyFolder = true;
        if (!isJar) allJars = false;
        if (info.type != RepoObjectType::Pointer) allPointers = false;
    }
    jarGroup_->setVisible(allJars && !anyFolder);
    pointerGroup_->setVisible(allPointers && !anyFolder);
    policyGroup_->setVisible(!anyFolder);
    deleteButton_->setVisible(!selection_.isEmpty());
}

void BatchEditorPanel::updatePartialEnabled()
{
    const QString mode = modeCombo_->currentData().toString();
    const bool partial = (mode == QLatin1String("partial")) && hasParser_;
    keysEdit_->setEnabled(partial);
    linesEdit_->setEnabled(partial);
}

void BatchEditorPanel::applyStyle()
{
    setStyleSheet(QStringLiteral(R"(
        QGroupBox {
            color: #e8eaed;
            border: 1px solid #454b54;
            border-radius: 6px;
            margin-top: 10px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #3a414b;
            color: #e8eaed;
            border: 1px solid #454b54;
            border-radius: 4px;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #454d59;
        }
        QLabel {
            color: #e8eaed;
        }
    )"));
}

} // namespace GUIWorker

#include "batch_editor_panel.moc"
