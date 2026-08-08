#include "export_page.h"
#include <build_engine.h>
#include <animated_progress.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <logger.h>
#include <IModpackExporter.h>

namespace GUIWorker {

ExportPage::ExportPage(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QString::fromUtf8("\u5bfc\u51fa\u6574\u5408\u5305"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(10);

    auto* outputGroup = new QGroupBox(QString::fromUtf8("\u5bfc\u51fa\u8bbe\u7f6e"), this);
    auto* outputLayout = new QVBoxLayout(outputGroup);

    auto* pathLayout = new QHBoxLayout();
    outputPathEdit_ = new QLineEdit(this);
    outputPathEdit_->setPlaceholderText(QString::fromUtf8("\u9009\u62e9\u5bfc\u51fa\u6587\u4ef6\u8def\u5f84..."));
    outputPathEdit_->setMinimumHeight(28);
    browseBtn_ = new QPushButton(QString::fromUtf8("\u6d4f\u89c8..."), this);
    pathLayout->addWidget(outputPathEdit_, 1);
    pathLayout->addWidget(browseBtn_);
    outputLayout->addLayout(pathLayout);

    auto* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel(QString::fromUtf8("\u5bfc\u51fa\u683c\u5f0f:"), this));
    formatCombo_ = new QComboBox(this);
    formatCombo_->setMinimumHeight(28);
    formatLayout->addWidget(formatCombo_, 1);
    outputLayout->addLayout(formatLayout);

    formatDescLabel_ = new QLabel("", this);
    formatDescLabel_->setWordWrap(true);
    formatDescLabel_->setStyleSheet(
        "color: #555; font-size: 12px; padding: 4px; "
        "background: #f9f9f9; border: 1px solid #eee; border-radius: 3px;");
    outputLayout->addWidget(formatDescLabel_);

    layout->addWidget(outputGroup);

    layout->addSpacing(8);

    auto* metaGroup = new QGroupBox(QString::fromUtf8("\u6574\u5408\u5305\u4fe1\u606f"), this);
    auto* metaLayout = new QFormLayout(metaGroup);

    nameEdit_ = new QLineEdit(this);
    nameEdit_->setPlaceholderText(QString::fromUtf8("\u4f8b\u5982: \u6211\u7684\u6574\u5408\u5305"));
    metaLayout->addRow(QString::fromUtf8("\u540d\u79f0:"), nameEdit_);

    versionEdit_ = new QLineEdit(this);
    versionEdit_->setPlaceholderText(QString::fromUtf8("\u4f8b\u5982: 1.0.0"));
    metaLayout->addRow(QString::fromUtf8("\u7248\u672c:"), versionEdit_);

    authorEdit_ = new QLineEdit(this);
    authorEdit_->setPlaceholderText(QString::fromUtf8("\u4f5c\u8005\u540d\u79f0"));
    metaLayout->addRow(QString::fromUtf8("\u4f5c\u8005:"), authorEdit_);

    descEdit_ = new QTextEdit(this);
    descEdit_->setPlaceholderText(QString::fromUtf8("\u6574\u5408\u5305\u63cf\u8ff0 (\u53ef\u9009)..."));
    descEdit_->setMaximumHeight(80);
    metaLayout->addRow(QString::fromUtf8("\u63cf\u8ff0:"), descEdit_);

    layout->addWidget(metaGroup);

    layout->addSpacing(8);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    exportBtn_ = new QPushButton(QString::fromUtf8("\u5bfc\u51fa"), this);
    exportBtn_->setMinimumWidth(120);
    exportBtn_->setMinimumHeight(32);
    exportBtn_->setStyleSheet(
        "QPushButton {"
        "  background-color: #0078d4; color: white;"
        "  border: none; border-radius: 4px;"
        "  font-weight: bold; padding: 6px 20px;"
        "}"
        "QPushButton:hover { background-color: #106ebe; }"
        "QPushButton:disabled { background-color: #ccc; }");
    btnLayout->addWidget(exportBtn_);
    layout->addLayout(btnLayout);

    progress_ = new AnimatedProgress(this);
    progress_->setVisible(false);
    layout->addWidget(progress_);

    statusLabel_ = new QLabel("", this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet("padding: 6px; font-size: 13px;");
    layout->addWidget(statusLabel_);

    layout->addStretch();

    connect(browseBtn_, &QPushButton::clicked,
        this, &ExportPage::onBrowseOutput);
    connect(exportBtn_, &QPushButton::clicked,
        this, &ExportPage::onExport);
    connect(formatCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &ExportPage::onFormatChanged);
}

void ExportPage::setBuildDir(QString dir)
{
    buildDir_ = dir;
}

void ExportPage::setOutputPath(const QString& path)
{
    if (path.isEmpty()) return;
    outputPathEdit_->setText(QDir::toNativeSeparators(path));
}

void ExportPage::setFormat(const QString& formatId)
{
    int idx = static_cast<int>(std::distance(
        availableFormats_.begin(),
        std::find(availableFormats_.begin(), availableFormats_.end(),
                  formatId.toStdString())));
    if (idx >= 0 && idx < static_cast<int>(availableFormats_.size())) {
        formatCombo_->setCurrentIndex(idx);
    }
}

void ExportPage::loadFormats()
{
    availableFormats_.clear();
    formatExtensions_.clear();
    formatDescriptions_.clear();
    formatCombo_->clear();

    struct Fmt {
        std::string id;
        std::string ext;
        std::string desc;
        QString display;
    };
    QList<Fmt> fmts;

    QDir exportersDir(QApplication::applicationDirPath() + QStringLiteral("/exporters"));
    if (exportersDir.exists()) {
        const QStringList files = exportersDir.entryList(
            {QStringLiteral("*.meta.json")}, QDir::Files, QDir::Name);
        for (const auto& f : files) {
            QFile file(exportersDir.filePath(f));
            if (!file.open(QIODevice::ReadOnly)) continue;
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
            file.close();
            if (parseError.error != QJsonParseError::NoError || !doc.isObject()) continue;
            QJsonObject obj = doc.object();
            Fmt fmt;
            fmt.id = obj.value(QStringLiteral("format")).toString().toStdString();
            fmt.ext = obj.value(QStringLiteral("extension")).toString().toStdString();
            fmt.desc = obj.value(QStringLiteral("description")).toString().toStdString();
            fmt.display = QString::fromStdString(fmt.id)
                + QStringLiteral(" (%1)").arg(QString::fromStdString(fmt.ext));
            if (!fmt.id.empty() && !fmt.ext.empty()) fmts.append(fmt);
        }
    }

    if (fmts.isEmpty()) {
        fmts.append({"mcbbs", ".zip",
            "MCBBS / PCL / HMCL \u901a\u7528\u6574\u5408\u5305\u683c\u5f0f\u3002",
            QString::fromUtf8("MCBBS \u6574\u5408\u5305 (.zip)")});
        fmts.append({"modrinth", ".mrpack",
            "Modrinth Modpack Format\u3002",
            QString::fromUtf8("Modrinth (.mrpack)")});
        fmts.append({"hmcl", ".zip",
            "HMCL \u539f\u751f\u683c\u5f0f\u3002",
            QString::fromUtf8("HMCL \u5de5\u4f5c\u76ee\u5f55")});
    }

    for (const auto& fmt : fmts) {
        availableFormats_.push_back(fmt.id);
        formatExtensions_.push_back(fmt.ext);
        formatDescriptions_.push_back(fmt.desc);
        formatCombo_->addItem(fmt.display);
    }

    if (!formatDescriptions_.empty()) {
        onFormatChanged(0);
    }
}

void ExportPage::onBrowseOutput()
{
    if (availableFormats_.empty()) {
        QMessageBox::information(this,
            QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u8bf7\u5148\u5b8c\u6210\u6784\u5efa\u518d\u8fdb\u884c\u5bfc\u51fa\u3002"));
        return;
    }

    int idx = formatCombo_->currentIndex();
    QString filter;
    if (idx >= 0 && idx < static_cast<int>(formatExtensions_.size())) {
        filter = QString::fromUtf8("\u6574\u5408\u5305\u6587\u4ef6 (*%1)").arg(
            QString::fromStdString(formatExtensions_[idx]));
    } else {
        filter = QString::fromUtf8("\u6240\u6709\u6587\u4ef6 (*.*)");
    }

    QString defaultPath = outputPathEdit_->text().isEmpty()
        ? QDir::homePath()
        : outputPathEdit_->text();

    QString path = QFileDialog::getSaveFileName(
        this,
        QString::fromUtf8("\u9009\u62e9\u5bfc\u51fa\u8def\u5f84"),
        defaultPath,
        filter);

    if (!path.isEmpty()) {
        if (idx >= 0 && idx < static_cast<int>(formatExtensions_.size())) {
            QString ext = QString::fromStdString(formatExtensions_[idx]);
            if (!path.endsWith(ext, Qt::CaseInsensitive)) {
                path += ext;
            }
        }
        outputPathEdit_->setText(QDir::toNativeSeparators(path));
    }
}

void ExportPage::onFormatChanged(int index)
{
    if (index >= 0 && index < static_cast<int>(formatDescriptions_.size())) {
        formatDescLabel_->setText(
            QString::fromStdString(formatDescriptions_[index]));
    } else {
        formatDescLabel_->setText("");
    }

    if (!outputPathEdit_->text().isEmpty() && index >= 0
        && index < static_cast<int>(formatExtensions_.size())) {
        QString current = outputPathEdit_->text();
        QFileInfo fi(current);
        QString suffix = fi.suffix().toLower();
        QString ext = QString::fromStdString(formatExtensions_[index]);
        QString expectedSuffix = ext.mid(1);

        if (suffix != expectedSuffix) {
            QString basePath = fi.absolutePath() + "/" + fi.completeBaseName();
            outputPathEdit_->setText(
                QDir::toNativeSeparators(basePath + ext));
        }
    }
}

void ExportPage::onExport()
{
    if (outputPathEdit_->text().isEmpty()) {
        QMessageBox::warning(this,
            QString::fromUtf8("\u8f93\u5165\u9519\u8bef"),
            QString::fromUtf8("\u8bf7\u9009\u62e9\u5bfc\u51fa\u8def\u5f84\u3002"));
        return;
    }

    if (nameEdit_->text().isEmpty()) {
        int ret = QMessageBox::question(this,
            QString::fromUtf8("\u63d0\u793a"),
            QString::fromUtf8("\u672a\u586b\u5199\u6574\u5408\u5305\u540d\u79f0\uff0c\u786e\u5b9a\u7ee7\u7eed\u5bfc\u51fa\uff1f"),
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    performExport();
}

NeoCore::ExportMetadata ExportPage::gatherMetadata() const
{
    NeoCore::ExportMetadata meta;
    meta.name = nameEdit_->text().toStdString();
    meta.version = versionEdit_->text().toStdString();
    meta.author = authorEdit_->text().toStdString();
    meta.description = descEdit_->toPlainText().toStdString();
    return meta;
}

void ExportPage::performExport()
{
    int idx = formatCombo_->currentIndex();
    if (idx < 0 || idx >= static_cast<int>(availableFormats_.size())) {
        QMessageBox::warning(this,
            QString::fromUtf8("\u9519\u8bef"),
            QString::fromUtf8("\u65e0\u6548\u7684\u5bfc\u51fa\u683c\u5f0f\u3002"));
        return;
    }

    std::string format = availableFormats_[idx];
    std::string outputPath = outputPathEdit_->text().toStdString();
    NeoCore::ExportMetadata meta = gatherMetadata();

statusLabel_->setStyleSheet(
        "padding: 6px; font-size: 13px; color: #0078d4;");
    statusLabel_->setText(QString::fromUtf8("\u6b63\u5728\u5bfc\u51fa..."));

    progress_->setVisible(true);
    progress_->setIndeterminate(true);
    progress_->setText(QString::fromUtf8("\u6b63\u5728\u5bfc\u51fa %1...").arg(
        QString::fromStdString(format)));
    progress_->startAnimation();

    emit exportStarted(QString::fromStdString(outputPath), QString::fromStdString(format));

    bool success = false;
    QString resultMsg;

    if (!buildDir_.isEmpty()) {
        NeoBuild::BuildEngine engine;
        if (engine.init(buildDir_.toStdString())) {
            success = engine.exportModpack(format, outputPath, meta);
        }
    } else {
        progress_->stopAnimation();
        progress_->setVisible(false);
        statusLabel_->setStyleSheet(
            "padding: 6px; font-size: 13px; color: red; font-weight: bold;");
        statusLabel_->setText(QString::fromUtf8(
            "\u5bfc\u51fa\u5931\u8d25: \u6784\u5efa\u76ee\u5f55\u4e3a\u7a7a\uff0c\u8bf7\u5148\u5b8c\u6210\u6784\u5efa\u3002"));
        CLogger::Error("Export failed: build dir is empty");
        return;
    }

    progress_->stopAnimation();
    progress_->setVisible(false);

    if (success) {
        resultMsg = QString::fromUtf8("\u5bfc\u51fa\u6210\u529f: %1").arg(
            QDir::toNativeSeparators(QString::fromStdString(outputPath)));
        statusLabel_->setStyleSheet(
            "padding: 6px; font-size: 13px; color: green; font-weight: bold;");
        CLogger::Info("Export succeeded: {}", outputPath);
    } else {
        resultMsg = QString::fromUtf8("\u5bfc\u51fa\u5931\u8d25: \u8bf7\u68c0\u67e5\u6784\u5efa\u8f93\u51fa\u548c\u65e5\u5fd7\u3002");
        statusLabel_->setStyleSheet(
            "padding: 6px; font-size: 13px; color: red; font-weight: bold;");
        CLogger::Error("Export failed");
    }

    statusLabel_->setText(resultMsg);
}

} // namespace GUIWorker

#include "export_page.moc"


