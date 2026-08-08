#include "config_file_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFile>
#include <QDir>
#include <QMessageBox>
#include <QApplication>
#include <QCheckBox>
#include <QTreeWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTextEdit>
#include <QRegularExpression>
#include <QCoreApplication>

#include <algorithm>

#include <PluginLoader.h>
#include <IConfigParser.h>
#include <logger.h>

namespace GUIWorker {

ConfigFileEditor::ConfigFileEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto* title = new QLabel(QString::fromUtf8("\u914d\u7f6e\u6587\u4ef6\u540c\u6b65"), this);
    title->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold; color: #e8eaed;"));

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #ffd54f; font-size: 11px;"));

    parserLabel_ = new QLabel(this);
    parserLabel_->setStyleSheet(QStringLiteral("color: #81c784; font-size: 11px;"));

    lay->addWidget(title);
    lay->addWidget(pathLabel_);
    lay->addWidget(stateLabel_);
    lay->addWidget(parserLabel_);

    auto* modeGroup = new QGroupBox(QString::fromUtf8("\u540c\u6b65\u6a21\u5f0f"), this);
    auto* modeLay = new QVBoxLayout(modeGroup);
    modeLay->setSpacing(6);

    fullRb_ = new QRadioButton(QString::fromUtf8("\u5168\u91cf\u8986\u76d6 (full)\uff1a\u76f4\u63a5\u5199\u5165\u76ee\u6807"), modeGroup);
    partialRb_ = new QRadioButton(QString::fromUtf8("\u90e8\u5206\u540c\u6b65 (partial)\uff1a\u53ea\u66f4\u65b0\u8ffd\u8e2a\u7684\u952e/\u884c\uff0c\u4fdd\u7559\u76ee\u6807\u672c\u5730\u5185\u5bb9"), modeGroup);
    ignoreRb_ = new QRadioButton(QString::fromUtf8("\u5ffd\u7565 (ignore)\uff1a\u4e0d\u5199\u76ee\u6807"), modeGroup);

    auto* modeBtn = new QButtonGroup(this);
    modeBtn->addButton(fullRb_, 0);
    modeBtn->addButton(partialRb_, 1);
    modeBtn->addButton(ignoreRb_, 2);

    modeLay->addWidget(fullRb_);
    modeLay->addWidget(partialRb_);
    modeLay->addWidget(ignoreRb_);
    lay->addWidget(modeGroup);

    auto* trackedGroup = new QGroupBox(QString::fromUtf8("\u8ffd\u8e2a\u7684\u952e (partial)"), this);
    auto* trackedLay = new QVBoxLayout(trackedGroup);

    keysList_ = new QListWidget(trackedGroup);
    keysList_->setAlternatingRowColors(true);
    keysList_->setMaximumHeight(150);
    trackedLay->addWidget(keysList_);

    auto* linesRow = new QHBoxLayout;
    auto* linesLabel = new QLabel(QString::fromUtf8("\u8ffd\u8e2a\u7684\u884c\u53f7 (\u9017\u53f7\u5206\u9694):"), trackedGroup);
    linesEdit_ = new QLineEdit(trackedGroup);
    linesEdit_->setPlaceholderText(QStringLiteral("1,5,9"));
    linesRow->addWidget(linesLabel);
    linesRow->addWidget(linesEdit_, 1);
    trackedLay->addLayout(linesRow);
    lay->addWidget(trackedGroup);

    auto* scopeBox = new QGroupBox(QString::fromUtf8("\u4fdd\u5b58\u8303\u56f4"), this);
    auto* scopeLay = new QVBoxLayout(scopeBox);
    topRb_ = new QRadioButton(QString::fromUtf8("\u9876\u5c42\u9ed8\u8ba4\uff08\u5168\u90e8\u5206\u652f\uff09"), scopeBox);
    branchRb_ = new QRadioButton(QString::fromUtf8("\u5f53\u524d\u5206\u652f\u8986\u76d6"), scopeBox);
    branchRb_->setChecked(true);
    scopeLay->addWidget(topRb_);
    scopeLay->addWidget(branchRb_);
    lay->addWidget(scopeBox);

    auto* btnRow = new QHBoxLayout;
    previewButton_ = new QPushButton(QString::fromUtf8("\u9884\u89c8 merge \u7ed3\u679c"), this);
    saveButton_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58"), this);
    btnRow->addWidget(previewButton_);
    btnRow->addStretch(1);
    btnRow->addWidget(saveButton_);
    lay->addLayout(btnRow);
    lay->addStretch(1);

    connect(modeBtn, &QButtonGroup::idClicked, this, [this](int) {
        updateEnabled();
    });
    connect(saveButton_, &QPushButton::clicked, this, [this]() {
        std::vector<std::string> keys;
        for (int i = 0; i < keysList_->count(); ++i) {
            auto* item = keysList_->item(i);
            if (item->checkState() == Qt::Checked) {
                keys.push_back(item->text().toStdString());
            }
        }
        std::vector<int> lines;
        const QStringList parts = linesEdit_->text().split(
            QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts);
        for (const auto& p : parts) {
            bool ok = false;
            const int v = p.toInt(&ok);
            if (ok && v > 0) lines.push_back(v);
        }
        QString mode;
        if (partialRb_->isChecked()) mode = QStringLiteral("partial");
        else if (ignoreRb_->isChecked()) mode = QStringLiteral("ignore");
        else mode = QStringLiteral("full");
        emit saveRequested(relativePath_, mode, keys, lines, branchRb_->isChecked());
        emit contentModified();
    });
    connect(previewButton_, &QPushButton::clicked, this,
        &ConfigFileEditor::doPreview);

    updateEnabled();
    applyStyle();
}

ConfigFileEditor::~ConfigFileEditor() = default;

void ConfigFileEditor::load(const QString& relativePath,
    const QString& absRepoPath, const QString& repoDir,
    const QString& branch, const QString& effectiveMode,
    const std::vector<std::string>& effectiveKeys,
    const std::vector<int>& effectiveLines,
    bool branchOverrides)
{
    relativePath_ = relativePath;
    absRepoPath_ = absRepoPath;
    repoDir_ = repoDir;
    branch_ = branch;

    pathLabel_->setText(relativePath);
    stateLabel_->setText(branchOverrides
        ? QString::fromUtf8("\u2714 \u5f53\u524d\u5206\u652f\u5df2\u8986\u76d6\u9876\u5c42\u8bbe\u7f6e")
        : QString::fromUtf8("\u2718 \u7ee7\u627f\u9876\u5c42\u8bbe\u7f6e\uff08\u672a\u5728\u672c\u5206\u652f\u8986\u76d6\uff09"));

    if (!loader_) {
        loader_ = std::make_unique<NeoCore::PluginLoader>();
        loader_->ScanDirectory(
            (QCoreApplication::applicationDirPath()
                + QStringLiteral("/parsers")).toStdString());
    }
    parser_ = loader_ ? loader_->FindParser(absRepoPath.toStdString()) : nullptr;
    if (parser_) {
        const auto cap = parser_->capability();
        parserLabel_->setText(QString::fromUtf8("\u2705 \u8bc6\u522b\u89e3\u6790\u5668: %1")
            .arg(QString::fromStdString(cap.name)));
    } else {
        parserLabel_->setText(QString::fromUtf8("\u26a0 \u672a\u627e\u5230\u5bf9\u5e94\u89e3\u6790\u5668\uff0cpartial \u5c06\u56de\u9000\u4e3a\u8986\u76d6\u5e76\u63d0\u793a"));
    }

    if (effectiveMode == QLatin1String("partial")) partialRb_->setChecked(true);
    else if (effectiveMode == QLatin1String("ignore")) ignoreRb_->setChecked(true);
    else fullRb_->setChecked(true);

    keysList_->clear();
    if (parser_) {
        reloadKeys();
        for (int i = 0; i < keysList_->count(); ++i) {
            auto* item = keysList_->item(i);
            const std::string key = item->text().toStdString();
            if (std::find(effectiveKeys.begin(), effectiveKeys.end(), key)
                != effectiveKeys.end()) {
                item->setCheckState(Qt::Checked);
            }
        }
    }

    QStringList lineParts;
    for (int v : effectiveLines) {
        lineParts << QString::number(v);
    }
    linesEdit_->setText(lineParts.join(QStringLiteral(", ")));

    updateEnabled();
}

void ConfigFileEditor::setScopeTop()
{
    topRb_->setChecked(true);
}

void ConfigFileEditor::reloadKeys()
{
    keysList_->clear();
    if (!parser_ || absRepoPath_.isEmpty()) return;
    std::vector<std::string> keys;
    try {
        keys = parser_->list_keys(absRepoPath_.toStdString());
    } catch (const std::exception& e) {
        CLogger::Error("ConfigFileEditor: list_keys exception: {}", e.what());
        return;
    }
    for (const auto& k : keys) {
        auto* item = new QListWidgetItem(QString::fromStdString(k), keysList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
}

void ConfigFileEditor::updateEnabled()
{
    const bool partial = partialRb_->isChecked();
    keysList_->setEnabled(partial);
    linesEdit_->setEnabled(partial);
}

void ConfigFileEditor::doPreview()
{
    if (!parser_) {
        QMessageBox::information(this,
            QString::fromUtf8("\u9884\u89c8"),
            QString::fromUtf8("\u6ca1\u6709\u53ef\u7528\u7684\u89e3\u6790\u5668\uff0c\u65e0\u6cd5\u751f\u6210 merge \u9884\u89c8\u3002"));
        return;
    }

    std::vector<std::string> keys;
    for (int i = 0; i < keysList_->count(); ++i) {
        auto* item = keysList_->item(i);
        if (item->checkState() == Qt::Checked) {
            keys.push_back(item->text().toStdString());
        }
    }

    QFile remoteFile(absRepoPath_);
    if (!remoteFile.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QString::fromUtf8("\u9884\u89c8"),
            QString::fromUtf8("\u65e0\u6cd5\u8bfb\u53d6\u6e90\u6587\u4ef6:\n%1").arg(absRepoPath_));
        return;
    }
    const QString remote = QString::fromUtf8(remoteFile.readAll());
    remoteFile.close();

    QString localPath = repoDir_ + QStringLiteral("/.minecraft/versions/")
        + branch_ + QLatin1Char('/') + relativePath_;
    QString local;
    if (QFile::exists(localPath)) {
        QFile f(localPath);
        if (f.open(QIODevice::ReadOnly)) {
            local = QString::fromUtf8(f.readAll());
            f.close();
        }
    }

    QString merged;
    try {
        merged = QString::fromStdString(parser_->merge_entries(
            absRepoPath_.toStdString(), keys,
            remote.toStdString(), local.toStdString()));
    } catch (const std::exception& e) {
        QMessageBox::warning(this, QString::fromUtf8("\u9884\u89c8"),
            QString::fromUtf8("merge \u5931\u8d25: %1").arg(QString::fromUtf8(e.what())));
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle(QString::fromUtf8("merge \u9884\u89c8 - %1").arg(relativePath_));
    dialog->resize(560, 420);
    auto* dlay = new QVBoxLayout(dialog);
    auto* info = new QLabel(QString::fromUtf8(
        "\u7eff\u8272 = \u6e90\u6587\u4ef6\u4f20\u64ad\u7684\u8ffd\u8e2a\u503c\uff08\u5148\u89c8\u8bf4\u660e\uff09\n"
        "\u8ffd\u8e2a %1 \u4e2a\u952e\uff1b\u76ee\u6807\u672c\u5730\u57fa\u7ebf: %2")
        .arg(keys.size())
        .arg(local.isEmpty()
            ? QString::fromUtf8("(\u65e0\uff0c\u5168\u91cf\u5f15\u5165\u6e90\u6587\u4ef6)")
            : QString::fromUtf8("%1 \u5b57\u7b26").arg(local.size())), dialog);
    auto* view = new QTextEdit(dialog);
    view->setReadOnly(true);
    view->setPlainText(merged);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    dlay->addWidget(info);
    dlay->addWidget(view, 1);
    dlay->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void ConfigFileEditor::applyStyle()
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
        QListWidget {
            background-color: #262a30;
            color: #d8dce2;
            border: 1px solid #3a4048;
            border-radius: 4px;
        }
        QLineEdit {
            background-color: #262a30;
            color: #d8dce2;
            border: 1px solid #3a4048;
            border-radius: 4px;
            padding: 3px 6px;
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

#include "config_file_editor.moc"
