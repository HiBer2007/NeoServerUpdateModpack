#include "serverconfig_rules_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QSet>
#include <QSignalBlocker>
#include <QCoreApplication>

#include <fstream>

#include <nlohmann/json.hpp>

#include <PluginLoader.h>
#include <IConfigParser.h>
#include <logger.h>

#include "sync_policy_display.h"

namespace GUIWorker {

namespace {

QString canonicalMode(const QString& mode)
{
    if (mode == QLatin1String("full") || mode == QLatin1String("merge")) {
        if (mode == QLatin1String("merge")) return QStringLiteral("partial");
        return QStringLiteral("overwrite");
    }
    if (mode == QLatin1String("overwrite")
        || mode == QLatin1String("partial")
        || mode == QLatin1String("ignore")) {
        return mode;
    }
    return QString();
}

} // namespace

ServerConfigRulesEditor::ServerConfigRulesEditor(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\u670d\u52a1\u7aef\u914d\u7f6e\u89c4\u5219\u7f16\u8f91\u5668"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #e8eaed;"));

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    pathLabel_->setWordWrap(true);

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #4dd0e1; font-size: 11px;"));
    stateLabel_->setWordWrap(true);

    hintLabel_ = new QLabel(this);
    hintLabel_->setStyleSheet(QStringLiteral("color: #8a9099; font-size: 11px;"));
    hintLabel_->setWordWrap(true);

    lay->addWidget(title);
    lay->addWidget(pathLabel_);
    lay->addWidget(stateLabel_);
    lay->addWidget(hintLabel_);

    auto* srcGroup = new QGroupBox(QString::fromUtf8("\u6e90\u6587\u4ef6\u5217\u8868 ([save]/serverconfig/)"), this);
    auto* srcLay = new QVBoxLayout(srcGroup);
    sourceList_ = new QListWidget(srcGroup);
    addSourceBtn_ = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u6e90\u6587\u4ef6..."), srcGroup);
    removeSourceBtn_ = new QPushButton(QString::fromUtf8("\u79fb\u9664\u6240\u9009\u6e90\u6587\u4ef6"), srcGroup);
    auto* srcBtnRow = new QHBoxLayout;
    srcBtnRow->addWidget(addSourceBtn_);
    srcBtnRow->addWidget(removeSourceBtn_);
    srcBtnRow->addStretch(1);
    srcLay->addWidget(sourceList_, 1);
    srcLay->addLayout(srcBtnRow);
    lay->addWidget(srcGroup, 1);

    auto* globleGroup = new QGroupBox(QString::fromUtf8("\u9ed8\u8ba4\u540c\u6b65\u8bbe\u7f6e (.rule/globle.json)"), this);
    auto* globleLay = new QHBoxLayout(globleGroup);
    globleLay->addWidget(new QLabel(QString::fromUtf8("default_mode:"), globleGroup));
    defaultModeCombo_ = new QComboBox(globleGroup);
    for (const auto& item : serverConfigModeDisplayItems()) {
        defaultModeCombo_->addItem(item.first, item.second);
    }
    defaultModeCombo_->setMaxVisibleItems(8);
    globleLay->addWidget(defaultModeCombo_, 1);
    globleLay->addWidget(new QLabel(
        QString::fromUtf8("\u672a\u5728\u6e05\u5355\u4e2d\u7684\u6587\u4ef6\u7684\u9ed8\u8ba4\u884c\u4e3a"), globleGroup));
    lay->addWidget(globleGroup);

    auto* listGroup = new QGroupBox(QString::fromUtf8("\u9010\u6587\u4ef6\u6a21\u5f0f\u8868 (.rule/list.json)"), this);
    auto* listLay = new QVBoxLayout(listGroup);
    modeTable_ = new QTableWidget(listGroup);
    modeTable_->setColumnCount(3);
    modeTable_->setHorizontalHeaderLabels({
        QString::fromUtf8("\u76f8\u5bf9\u8def\u5f84"),
        QString::fromUtf8("\u6a21\u5f0f"),
        QString::fromUtf8("\u89e3\u6790\u5668\u63d0\u793a"),
    });
    modeTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    modeTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    modeTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    modeTable_->verticalHeader()->setVisible(false);
    modeTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    listLay->addWidget(modeTable_);
    lay->addWidget(listGroup, 2);

    auto* reservedGroup = new QGroupBox(QString::fromUtf8("\u89c4\u5219\u6587\u4ef6\u7ec4 (\u9884\u7559\uff0c\u4ec5\u8bfb)"), this);
    auto* reservedLay = new QVBoxLayout(reservedGroup);
    reservedView_ = new QTextEdit(reservedGroup);
    reservedView_->setReadOnly(true);
    reservedView_->setMaximumHeight(90);
    reservedView_->setStyleSheet(QStringLiteral(
        "background-color: #262a30; color: #c8ccd2; border: 1px solid #3a4048;"));
    reservedLay->addWidget(reservedView_);
    lay->addWidget(reservedGroup);

    auto* btnRow = new QHBoxLayout;
    saveButton_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u89c4\u5219"), this);
    btnRow->addWidget(saveButton_);
    btnRow->addStretch(1);
    lay->addLayout(btnRow);

    connect(addSourceBtn_, &QPushButton::clicked, this,
        &ServerConfigRulesEditor::onAddSourceFile);
    connect(removeSourceBtn_, &QPushButton::clicked, this,
        &ServerConfigRulesEditor::onRemoveSourceFile);
    connect(saveButton_, &QPushButton::clicked, this,
        &ServerConfigRulesEditor::onSave);
    connect(modeTable_, &QTableWidget::cellChanged, this,
        &ServerConfigRulesEditor::onModeChanged);
}

ServerConfigRulesEditor::~ServerConfigRulesEditor() = default;

QString ServerConfigRulesEditor::scDir() const
{
    return repoDir_ + QStringLiteral("/branches/") + branch_
        + QStringLiteral("/[save]/serverconfig");
}

QString ServerConfigRulesEditor::ruleDir() const
{
    return scDir() + QStringLiteral("/.rule");
}

void ServerConfigRulesEditor::setContext(const QString& repoDir,
    const QString& branch)
{
    repoDir_ = repoDir;
    branch_ = branch;
}

void ServerConfigRulesEditor::load()
{
    pathLabel_->setText(scDir());
    if (!QDir(scDir()).exists()) {
        stateLabel_->setText(QString::fromUtf8(
            "\u672a\u521b\u5efa\uff1a\u6e90\u6587\u4ef6\u5939\u4e0d\u5b58\u5728\uff0c\u4fdd\u5b58\u65f6\u81ea\u52a8\u5efa\u7acb\u3002"));
        hintLabel_->setText(QString::fromUtf8(
            "\u6dfb\u52a0\u6e90\u6587\u4ef6\u540e\u4fdd\u5b58\u5373\u521b\u5efa\u6574\u4e2a\u76ee\u5f55\u7ed3\u6784\u3002"));
    } else {
        stateLabel_->setText(QString::fromUtf8(
            "\u76ee\u6807\u4e3a\u6bcf\u4e2a\u5b58\u6863\u7684 serverconfig/\u76ee\u5f55\uff0c\u6e90\u6587\u4ef6=镜像\u5185\u5bb9\u3002"));
        hintLabel_->setText(QString::fromUtf8(
            "overwrite \u6574\u6587\u4ef6\u8986\u76d6\uff1bpartial \u534a\u540c\u6b65 merge\uff08\u9700\u5bf9\u5e94\u89e3\u6790\u5668\uff0c\u5426\u5219\u56de\u9000\u8986\u76d6\uff09\uff1bignore \u4e0d\u78b0\u3002"));
    }

    loader_.reset();
    loader_ = std::make_unique<NeoCore::PluginLoader>();
    loader_->ScanDirectory(
        (QApplication::applicationDirPath()
            + QStringLiteral("/parsers")).toStdString());

    listedModes_.clear();
    globleVersion_.clear();
    globleDescription_.clear();
    sourceFiles_.clear();
    reservedFiles_.clear();

    loadGloble();
    loadList();
    loadSourceFiles();
    loadReserved();
    buildModeTable();
}

void ServerConfigRulesEditor::loadGloble()
{
    QFile f(ruleDir() + QStringLiteral("/globle.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        defaultModeCombo_->setCurrentIndex(0);
        return;
    }
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        const QString mode = canonicalMode(
            QString::fromStdString(j.value("default_mode", "overwrite")));
        const int idx = defaultModeCombo_->findData(mode);
        defaultModeCombo_->setCurrentIndex(idx < 0 ? 0 : idx);
        globleVersion_ = QString::fromStdString(j.value("version", ""));
        globleDescription_ = QString::fromStdString(j.value("description", ""));
    } catch (...) {
        f.close();
        defaultModeCombo_->setCurrentIndex(0);
    }
}

void ServerConfigRulesEditor::loadList()
{
    QFile f(ruleDir() + QStringLiteral("/list.json"));
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        if (j.contains("files") && j["files"].is_object()) {
            for (const auto& [rel, mode] : j["files"].items()) {
                if (!mode.is_string()) continue;
                const QString canonical = canonicalMode(
                    QString::fromStdString(mode.get<std::string>()));
                if (!canonical.isEmpty()) {
                    listedModes_[QString::fromStdString(rel)] = canonical;
                }
            }
        }
    } catch (...) {
        f.close();
    }
}

void ServerConfigRulesEditor::loadSourceFiles()
{
    sourceFiles_.clear();
    QDir dir(scDir());
    if (!dir.exists()) {
        return;
    }
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& fi : entries) {
        if (fi.fileName() == QLatin1String(".rule")) continue;
        sourceFiles_ << fi.fileName();
    }
    sourceList_->clear();
    sourceList_->addItems(sourceFiles_);
}

void ServerConfigRulesEditor::loadReserved()
{
    reservedFiles_.clear();
    QDir dir(ruleDir());
    if (!dir.exists()) {
        reservedView_->setPlainText(QString::fromUtf8("(\u65e0)"));
        return;
    }
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    QStringList text;
    for (const auto& fi : entries) {
        if (fi.fileName() == QLatin1String("globle.json")
            || fi.fileName() == QLatin1String("list.json")) {
            continue;
        }
        reservedFiles_ << fi.fileName();
        text << QStringLiteral("- ") + fi.fileName();
    }
    reservedView_->setPlainText(text.isEmpty()
        ? QString::fromUtf8("(\u65e0)")
        : text.join(QLatin1Char('\n')));
}

void ServerConfigRulesEditor::buildModeTable()
{
    QSet<QString> rels;
    for (const auto& name : sourceFiles_) {
        rels.insert(name);
    }
    for (const auto& [rel, mode] : listedModes_) {
        Q_UNUSED(mode);
        rels.insert(rel);
    }
    QStringList sorted = rels.values();
    sorted.sort();

    QSignalBlocker blocker(modeTable_);
    modeTable_->setRowCount(sorted.size());
    for (int i = 0; i < sorted.size(); ++i) {
        const QString rel = sorted[i];
        auto* relItem = new QTableWidgetItem(rel);
        relItem->setFlags(relItem->flags() & ~Qt::ItemIsEditable);
        modeTable_->setItem(i, 0, relItem);

        auto* combo = new QComboBox(modeTable_);
        for (const auto& item : serverConfigModeDisplayItems()) {
            combo->addItem(item.first, item.second);
        }
        combo->setMaxVisibleItems(8);
        const QString mode = listedModes_.count(rel)
            ? listedModes_[rel] : defaultModeCombo_->currentData().toString();
        const int idx = combo->findData(mode);
        combo->setCurrentIndex(idx < 0 ? 0 : idx);
        modeTable_->setCellWidget(i, 1, combo);

        auto* hint = new QTableWidgetItem();
        hint->setFlags(Qt::ItemIsEnabled);
        modeTable_->setItem(i, 2, hint);
        updateParserHint(i);
    }
    blocker.unblock();
}

void ServerConfigRulesEditor::updateParserHint(int row)
{
    auto* combo = qobject_cast<QComboBox*>(modeTable_->cellWidget(row, 1));
    auto* hint = modeTable_->item(row, 2);
    if (!combo || !hint) return;
    const QString rel = modeTable_->item(row, 0)->text();
    const QString mode = combo->currentData().toString();
    if (mode != QLatin1String("partial")) {
        hint->setText(QString());
        return;
    }
    const QString absPath = scDir() + QLatin1Char('/') + rel;
    NeoCore::IConfigParser* parser = loader_
        ? loader_->FindParser(absPath.toStdString()) : nullptr;
    if (parser) {
        const auto cap = parser->capability();
        hint->setText(QString::fromUtf8("\u2705 %1")
            .arg(QString::fromStdString(cap.name)));
    } else {
        hint->setText(QString::fromUtf8(
            "\u26a0 \u65e0\u89e3\u6790\u5668\uff0c\u5c06\u56de\u9000\u4e3a\u8986\u76d6"));
    }
}

void ServerConfigRulesEditor::onModeChanged(int row, int column)
{
    if (column == 1) {
        updateParserHint(row);
    }
}

void ServerConfigRulesEditor::onAddSourceFile()
{
    const QStringList files = QFileDialog::getOpenFileNames(this,
        QString::fromUtf8("\u9009\u62e9\u6e90\u914d\u7f6e\u6587\u4ef6"), QDir::homePath());
    if (files.isEmpty()) return;

    QDir().mkpath(scDir());
    QStringList added;
    for (const auto& src : files) {
        const QString name = QFileInfo(src).fileName();
        const QString dst = scDir() + QLatin1Char('/') + name;
        if (QFile::exists(dst)) {
            appendLog(QString::fromUtf8("\u2718 \u5df2\u5b58\u5728\uff0c\u8df3\u8fc7: %1").arg(name));
            continue;
        }
        if (QFile::copy(src, dst)) {
            added << dst;
            appendLog(QString::fromUtf8("\u2705 \u5df2\u590d\u5236: %1").arg(name));
        } else {
            appendLog(QString::fromUtf8("\u2718 \u590d\u5236\u5931\u8d25: %1").arg(name));
        }
    }
    if (!added.isEmpty()) {
        emit gitAddRequested(added);
    }
    loadSourceFiles();
    buildModeTable();
}

void ServerConfigRulesEditor::onRemoveSourceFile()
{
    auto* item = sourceList_->currentItem();
    if (!item) {
        QMessageBox::information(this, QString::fromUtf8("\u79fb\u9664\u6e90\u6587\u4ef6"),
            QString::fromUtf8("\u8bf7\u5148\u5728\u5217\u8868\u4e2d\u9009\u62e9\u8981\u79fb\u9664\u7684\u6587\u4ef6\u3002"));
        return;
    }
    const QString name = item->text();
    const QString dst = scDir() + QLatin1Char('/') + name;
    if (!QFile::remove(dst)) {
        QMessageBox::warning(this, QString::fromUtf8("\u79fb\u9664\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u5220\u9664:\n%1").arg(dst));
        return;
    }
    emit gitAddRequested({ dst });
    appendLog(QString::fromUtf8("\u2705 \u5df2\u79fb\u9664\u6e90\u6587\u4ef6: %1").arg(name));
    loadSourceFiles();
    buildModeTable();
}

void ServerConfigRulesEditor::onSave()
{
    QDir().mkpath(ruleDir());

    nlohmann::json globle;
    globle["default_mode"] = defaultModeCombo_->currentData().toString().toStdString();
    if (!globleVersion_.isEmpty()) {
        globle["version"] = globleVersion_.toStdString();
    }
    if (!globleDescription_.isEmpty()) {
        globle["description"] = globleDescription_.toStdString();
    }
    {
        std::ofstream f((ruleDir() + QStringLiteral("/globle.json")).toStdString());
        if (!f.is_open()) {
            QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
                QString::fromUtf8("\u65e0\u6cd5\u5199\u5165 globle.json\u3002"));
            return;
        }
        f << globle.dump(2) << std::endl;
        f.close();
    }

    const QString defaultMode = defaultModeCombo_->currentData().toString();
    nlohmann::json files = nlohmann::json::object();
    for (int i = 0; i < modeTable_->rowCount(); ++i) {
        auto* relItem = modeTable_->item(i, 0);
        auto* combo = qobject_cast<QComboBox*>(modeTable_->cellWidget(i, 1));
        if (!relItem || !combo) continue;
        const QString rel = relItem->text().trimmed();
        if (rel.isEmpty()) continue;
        const QString mode = combo->currentData().toString();
        if (mode.isEmpty()) continue;
        files[rel.toStdString()] = mode.toStdString();
    }
    nlohmann::json list;
    list["files"] = files;
    {
        std::ofstream f((ruleDir() + QStringLiteral("/list.json")).toStdString());
        if (!f.is_open()) {
            QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
                QString::fromUtf8("\u65e0\u6cd5\u5199\u5165 list.json\u3002"));
            return;
        }
        f << list.dump(2) << std::endl;
        f.close();
    }

    emit gitAddRequested({
        ruleDir() + QStringLiteral("/globle.json"),
        ruleDir() + QStringLiteral("/list.json"),
    });
    appendLog(QString::fromUtf8("\u2705 \u89c4\u5219\u5df2\u4fdd\u5b58 (default=%1, files=%2)")
        .arg(defaultMode).arg(files.size()));
}

void ServerConfigRulesEditor::appendLog(const QString& line)
{
    emit logMessage(line);
    CLogger::Info("ServerConfigRulesEditor: {}", line.toStdString());
}

} // namespace GUIWorker

#include "serverconfig_rules_editor.moc"
