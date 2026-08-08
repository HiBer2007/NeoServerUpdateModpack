#include "build_page.h"
#include "animated_progress.h"

#include <modpack_exporter.h>
#include <platform_api.h>
#include <workspace_manager.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QTime>
#include <QDir>
#include <QDateTime>
#include <QTimer>
#include <QCoreApplication>
#include <QTextCursor>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QMetaObject>
#include <iostream>
#include <filesystem>
#include <logger.h>

namespace GUIWorker {

namespace fs = std::filesystem;

BuildPage::BuildPage(QWidget* parent)
    : QWidget(parent)
    , buildInProgress_(false)
    , detailsVisible_(false)
{
    const QColor windowBg = palette().color(QPalette::Window);
    const bool darkMode = windowBg.lightness() < 128;

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel(QString::fromUtf8("\u6784\u5efa\u6574\u5408\u5305"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    layout->addSpacing(8);

    mainLabel_ = new QLabel(QString::fromUtf8("\u51c6\u5907\u6784\u5efa..."), this);
    mainLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
        .arg(darkMode ? QStringLiteral("#9da2aa") : QStringLiteral("#666")));
    layout->addWidget(mainLabel_);

    mainBar_ = new QProgressBar(this);
    mainBar_->setRange(0, 100);
    mainBar_->setValue(0);
    mainBar_->setTextVisible(true);
    mainBar_->setFixedHeight(18);
    layout->addWidget(mainBar_);

    layout->addSpacing(6);

    subLabel_ = new QLabel(this);
    subLabel_->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;")
        .arg(darkMode ? QStringLiteral("#7a7f88") : QStringLiteral("#888")));
    layout->addWidget(subLabel_);

    subBar_ = new QProgressBar(this);
    subBar_->setRange(0, 1);
    subBar_->setValue(0);
    subBar_->setTextVisible(true);
    subBar_->setFixedHeight(12);
    layout->addWidget(subBar_);

    layout->addSpacing(8);

    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    detailsBtn_ = new QPushButton(QString::fromUtf8("\u5c55\u5f00\u8be6\u60c5"), this);
    detailsBtn_->setMinimumWidth(110);
    detailsBtn_->setMinimumHeight(32);
    btnRow->addWidget(detailsBtn_);

    cancelBtn_ = new QPushButton(QString::fromUtf8("\u53d6\u6d88\u6784\u5efa"), this);
    cancelBtn_->setMinimumWidth(120);
    cancelBtn_->setMinimumHeight(32);
    cancelBtn_->setEnabled(false);
    btnRow->addWidget(cancelBtn_);
    layout->addLayout(btnRow);

    logContainer_ = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logContainer_);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logOutput_ = new QTextEdit(logContainer_);
    logOutput_->setReadOnly(true);
    logOutput_->setFont(QFont("Consolas", 9));
    logOutput_->setStyleSheet(QStringLiteral(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #333; border-radius: 4px; padding: 4px; }"));
    logOutput_->setMaximumHeight(180);
    logLayout->addWidget(logOutput_);
    logContainer_->hide();
    layout->addWidget(logContainer_);

    layout->addStretch();

    connect(cancelBtn_, &QPushButton::clicked, this, &BuildPage::onCancelClicked);
    connect(detailsBtn_, &QPushButton::clicked, this, &BuildPage::onToggleDetails);

    // 跨线程回传（IBuildProgress 在 worker 线程调用）：显式 QueuedConnection 保证 GUI 线程更新
    connect(this, &BuildPage::logLine, this, &BuildPage::appendLog, Qt::QueuedConnection);
    connect(this, &BuildPage::progressUpdated, this,
        &BuildPage::onProgressUpdated, Qt::QueuedConnection);
    connect(this, &BuildPage::subBarUpdate, this,
        &BuildPage::onSubBarUpdate, Qt::QueuedConnection);
    connect(this, &BuildPage::buildFinished, this,
        &BuildPage::onBuildDone, Qt::QueuedConnection);
}

void BuildPage::startBuild(QString repoDir, QString gitBranch, QString modpackBranch,
    QString exportFormat, QString outputPath)
{
    repoDir_ = repoDir;
    gitBranch_ = gitBranch;
    modpackBranch_ = modpackBranch;
    exportFormat_ = exportFormat;
    outputPath_ = outputPath;
    buildWarnings_.clear();

    logOutput_->clear();
    cancelToken_.reset();
    cancelBtn_->setEnabled(true);
    buildInProgress_ = true;
    buildOutputDir_ = QString();
    detailsVisible_ = false;
    logContainer_->hide();
    detailsBtn_->setText(QString::fromUtf8("\u5c55\u5f00\u8be6\u60c5"));

    mainBar_->setValue(0);
    mainBar_->setFormat(QStringLiteral("%p%"));
    mainLabel_->setText(QString::fromUtf8("\u51c6\u5907\u6784\u5efa..."));
    subBar_->setRange(0, 1);
    subBar_->setValue(0);
    subBar_->setFormat(QString());
    subLabel_->setText(QString());

    appendLog(QString("[%1] \u5f00\u59cb\u6784\u5efa\u6574\u5408\u5305")
        .arg(QTime::currentTime().toString("HH:mm:ss")));
    appendLog(QString("[%1] \u4ed3\u5e93: %2").arg(
        QTime::currentTime().toString("HH:mm:ss"), repoDir_));
    appendLog(QString("[%1] Git \u5206\u652f: %2").arg(
        QTime::currentTime().toString("HH:mm:ss"), gitBranch_));
    appendLog(QString("[%1] \u6574\u5408\u5305\u5206\u652f: %2").arg(
        QTime::currentTime().toString("HH:mm:ss"), modpackBranch_));
    appendLog(QString("[%1] \u5bfc\u51fa\u683c\u5f0f: %2").arg(
        QTime::currentTime().toString("HH:mm:ss"), exportFormat_));
    appendLog(QString("[%1] \u8f93\u51fa\u76ee\u5f55: %2").arg(
        QTime::currentTime().toString("HH:mm:ss"), outputPath_));
    appendLog("");

    CLogger::Info("Build start: repo={} git_branch={} modpack={} format={} output={}",
        repoDir_.toUtf8().constData(), gitBranch_.toUtf8().constData(),
        modpackBranch_.toUtf8().constData(), exportFormat_.toUtf8().constData(),
        outputPath_.toUtf8().constData());

    if (workerThread_.joinable()) {
        workerThread_.join();
    }
    workerThread_ = std::thread([this]() { runBuildWorker(); });
}

void BuildPage::cancelBuild()
{
    cancelToken_.request_cancel();
}

void BuildPage::onCancelClicked()
{
    cancelToken_.request_cancel();
    cancelBtn_->setEnabled(false);

    QString timeStr = QTime::currentTime().toString("HH:mm:ss");
    appendLog(QString("[%1] \u6784\u5efa\u5df2\u53d6\u6d88").arg(timeStr));
    mainLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5df2\u53d6\u6d88"));

    buildInProgress_ = false;
    emit buildFinished(false, QString::fromUtf8("\u6784\u5efa\u5df2\u53d6\u6d88"),
        buildWarnings_);
}

void BuildPage::onToggleDetails()
{
    detailsVisible_ = !detailsVisible_;
    logContainer_->setVisible(detailsVisible_);
    detailsBtn_->setText(detailsVisible_ ? QString::fromUtf8("\u6536\u8d77\u8be6\u60c5")
                                         : QString::fromUtf8("\u5c55\u5f00\u8be6\u60c5"));
    emit logLine(QString());
}

void BuildPage::runBuildWorker()
{
    QStringList warnings;
    try {
        NeoBuild::ModpackExporter exporter;
        std::string exportersDir = (fs::path(
            QCoreApplication::applicationDirPath().toStdString()) / "exporters").string();
        if (fs::exists(exportersDir)) {
            CLogger::Debug("Build: scanning exporter dir {}", exportersDir);
            exporter.scanExporters(exportersDir);
        }

        NeoCore::IModpackExporter* plugin =
            exporter.exporterForFormat(exportFormat_.toStdString());
        if (!plugin) {
            CLogger::Error("Build: exporter plugin not found: {}", exportFormat_.toUtf8().constData());
            emit buildFinished(false,
                QString::fromUtf8("\u672a\u627e\u5230\u5bfc\u51fa\u63d2\u4ef6: ") + exportFormat_,
                warnings);
            return;
        }
        CLogger::Info("Build: using exporter plugin {}", exportFormat_.toUtf8().constData());

        NeoCore::BuildTarget target;
        target.workspace_path = repoDir_.toStdString();
        target.workspace_json = (fs::path(repoDir_.toStdString()) / "workspace.json").string();
        target.cache_dir = NeoBuild::getCacheDir();
        target.branch = modpackBranch_.toStdString();
        target.sync_to_directory = (exportFormat_ == QLatin1String("hmcl"));
        target.output_path = outputPath_.toStdString();

        NeoWorkspace::WorkspaceManager wm;
        if (wm.loadFromFile(target.workspace_json)) {
            target.metadata.name = wm.workspaceName();
            target.metadata.game_version = wm.minecraftVersion();
            target.metadata.modloader = wm.modloader();
        }
        if (target.metadata.name.empty()) {
            target.metadata.name = modpackBranch_.toStdString();
        }

        GuiBuildProgress progress(this, &cancelToken_);
        NeoCore::BuildResult result =
            plugin->build_modpack(target, &progress, &cancelToken_);

        if (cancelToken_.is_cancelled()) {
            emit buildFinished(false,
                QString::fromUtf8("\u6784\u5efa\u5df2\u53d6\u6d88"), warnings);
            return;
        }

        for (const auto& w : result.warnings) {
            warnings << QString::fromUtf8(w.c_str());
        }

        if (!result.success) {
            QString err = QString::fromUtf8(result.errorMessage.c_str());
            if (err.isEmpty()) err = QString::fromUtf8("\u672a\u77e5\u9519\u8bef\u3002");
            emit buildFinished(false, err, warnings);
            return;
        }

        QString outDir = QString::fromUtf8(result.outputDir.c_str());
        if (outDir.isEmpty()) outDir = outputPath_;
        emit buildFinished(true, outDir, warnings);
    } catch (const std::exception& e) {
        emit buildFinished(false, QString::fromUtf8(e.what()), warnings);
    }
}

void BuildPage::onBuildDone(bool success, const QString& message,
    const QStringList& warnings)
{
    buildInProgress_ = false;
    cancelBtn_->setEnabled(false);
    buildWarnings_ = warnings;

    QString timeStr = QTime::currentTime().toString("HH:mm:ss");

    if (success) {
        buildOutputDir_ = message;
        mainBar_->setValue(100);
        mainLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5b8c\u6210"));
        appendLog(QString("[%1] *** \u6784\u5efa\u6210\u529f ***").arg(timeStr));
        appendLog(QString("[%1] \u8f93\u51fa\u76ee\u5f55: %2").arg(timeStr, message));
        if (!warnings.isEmpty()) {
            appendLog(QString("[%1] \u4f34\u968f %2 \u4e2a\u8b66\u544a:")
                .arg(timeStr).arg(warnings.size()));
            for (const auto& w : warnings) {
                appendLog(QStringLiteral("  [\u8b66\u544a] %1").arg(w));
            }
        }
        CLogger::Info("Build succeeded: {}", message.toUtf8().constData());
    } else {
        mainLabel_->setText(QString::fromUtf8("\u6784\u5efa\u5931\u8d25"));
        appendLog(QString("[%1] *** \u6784\u5efa\u5931\u8d25: %2 ***").arg(timeStr, message));
        if (!warnings.isEmpty()) {
            appendLog(QStringLiteral("[%1] \u8b66\u544a:").arg(timeStr));
            for (const auto& w : warnings) {
                appendLog(QStringLiteral("  [\u8b66\u544a] %1").arg(w));
            }
        }
        CLogger::Error("Build failed: {}", message.toUtf8().constData());
    }
}

void BuildPage::onProgressUpdated(QString stage, int percent, QString message)
{
    if (cancelToken_.is_cancelled()) return;

    mainBar_->setValue(percent);
    mainLabel_->setText(QStringLiteral("[%1] %2").arg(stage, message));

    QCoreApplication::processEvents();
}

void BuildPage::onSubBarUpdate(int handle, QString label, int percent, QString info)
{
    Q_UNUSED(handle)

    if (percent >= 0) {
        subBar_->setRange(0, 100);
        subBar_->setValue(percent);
        subBar_->setFormat(QStringLiteral("%1%").arg(percent));
    }
    if (!info.isEmpty()) {
        subLabel_->setText(QStringLiteral("%1: %2").arg(label, info));
    } else if (percent >= 0) {
        subLabel_->setText(QStringLiteral("%1: %2%").arg(label).arg(percent));
    }

    QCoreApplication::processEvents();
}

void BuildPage::appendLog(const QString& msg)
{
    if (!msg.isEmpty()) {
        logOutput_->append(msg);
        QTextCursor cursor = logOutput_->textCursor();
        cursor.movePosition(QTextCursor::End);
        logOutput_->setTextCursor(cursor);
    }

    std::cout << msg.toStdString() << std::endl;
}

// ---- GuiBuildProgress（worker 线程 → 信号 queued 到 GUI 线程） ----

void BuildPage::GuiBuildProgress::set_main_stage(const std::string& stage)
{
    stage_ = stage;
    emit page_->progressUpdated(QString::fromUtf8(stage_.c_str()), percent_,
        QString::fromUtf8(message_.c_str()));
}

void BuildPage::GuiBuildProgress::set_main_progress(int percent)
{
    percent_ = percent;
    emit page_->progressUpdated(QString::fromUtf8(stage_.c_str()), percent_,
        QString::fromUtf8(message_.c_str()));
}

void BuildPage::GuiBuildProgress::set_main_message(const std::string& message)
{
    message_ = message;
    emit page_->progressUpdated(QString::fromUtf8(stage_.c_str()), percent_,
        QString::fromUtf8(message_.c_str()));
}

int BuildPage::GuiBuildProgress::add_sub_bar(const std::string& label)
{
    int handle = nextHandle_++;
    labels_[handle] = label;
    emit page_->subBarUpdate(handle, QString::fromUtf8(label.c_str()), 0, QString());
    return handle;
}

void BuildPage::GuiBuildProgress::remove_sub_bar(int handle)
{
    labels_.erase(handle);
}

void BuildPage::GuiBuildProgress::set_sub_progress(int handle, int percent)
{
    auto it = labels_.find(handle);
    if (it == labels_.end()) return;
    emit page_->subBarUpdate(handle, QString::fromUtf8(it->second.c_str()), percent, QString());
}

void BuildPage::GuiBuildProgress::set_sub_info(int handle, const std::string& info)
{
    auto it = labels_.find(handle);
    if (it == labels_.end()) return;
    emit page_->subBarUpdate(handle, QString::fromUtf8(it->second.c_str()), -1,
        QString::fromUtf8(info.c_str()));
}

void BuildPage::GuiBuildProgress::log(const std::string& line)
{
    emit page_->logLine(QString::fromUtf8(line.c_str()));
}

bool BuildPage::GuiBuildProgress::is_cancelled() const
{
    return token_ && token_->is_cancelled();
}

} // namespace GUIWorker

#include "build_page.moc"
