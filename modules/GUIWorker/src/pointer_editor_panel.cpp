#include "pointer_editor_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QFont>
#include <QScrollArea>

#include <fstream>
#include <thread>
#include <atomic>

#include <zip.h>
#include <nlohmann/json.hpp>

#include <pointer_downloader.h>
#include <logger.h>

namespace GUIWorker {

namespace {

struct BatchStats {
    std::atomic<int> done{0};
    std::atomic<int> success{0};
    std::atomic<int> failed{0};
    QVector<ConvertedItem> converted;
};

std::string computeFileSha256(const QString& fp)
{
    QFile f(fp);
    if (!f.open(QIODevice::ReadOnly)) return "";
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f);
    f.close();
    return h.result().toHex().toStdString();
}

std::string extractModIdFromJar(const std::string& jarPath)
{
    int err = 0;
    zip_t* a = zip_open(jarPath.c_str(), ZIP_RDONLY, &err);
    if (!a) return "";
    auto read = [&](const char* e) -> std::string {
        zip_stat_t s;
        if (zip_stat(a, e, 0, &s) != 0) return "";
        zip_file_t* zf = zip_fopen(a, e, 0);
        if (!zf) return "";
        std::vector<char> buf(s.size + 1);
        zip_fread(zf, buf.data(), s.size);
        buf[s.size] = 0;
        zip_fclose(zf);
        return std::string(buf.data());
    };
    std::string raw = read("fabric.mod.json");
    if (!raw.empty()) {
        try {
            auto j = nlohmann::json::parse(raw);
            zip_close(a);
            return j.value("id", "");
        } catch (...) {}
    }
    raw = read("mcmod.info");
    if (!raw.empty()) {
        try {
            auto j = nlohmann::json::parse(raw);
            zip_close(a);
            if (j.is_array() && !j.empty()) return j[0].value("modid", "");
            if (j.is_object()) return j.value("modid", "");
        } catch (...) {}
    }
    raw = read("META-INF/mods.toml");
    if (raw.empty()) raw = read("META-INF/neoforge.mods.toml");
    if (!raw.empty()) {
        auto p = raw.find("modId");
        if (p != std::string::npos) {
            auto eq = raw.find('=', p);
            if (eq != std::string::npos && eq < p + 30) {
                auto v = raw.substr(eq + 1);
                auto nl = v.find('\n');
                if (nl != std::string::npos) v = v.substr(0, nl);
                v.erase(0, v.find_first_not_of(" \"\t\r\n"));
                v.erase(v.find_last_not_of(" \"\t\r\n") + 1);
                zip_close(a);
                if (!v.empty()) return v;
            }
        }
    }
    zip_close(a);
    return "";
}

bool searchModrinth(const std::string& modId, std::string& oPid,
    std::string& oVid)
{
    QUrl url(QString::fromStdString(
        "https://api.modrinth.com/v2/project/" + modId + "/version"));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0");
    req.setTransferTimeout(15000);
    QNetworkAccessManager mgr;
    QNetworkReply* r = mgr.get(req);
    QEventLoop loop;
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (r->error() != QNetworkReply::NoError) {
        r->deleteLater();
        return false;
    }
    const QByteArray d = r->readAll();
    r->deleteLater();
    try {
        auto arr = nlohmann::json::parse(d.toStdString());
        if (!arr.is_array() || arr.empty()) return false;
        oPid = modId;
        oVid = arr[0].value("id", "");
        return !oVid.empty();
    } catch (...) {}
    return false;
}

} // namespace

PointerEditorPanel::PointerEditorPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 16, 12);
    lay->setSpacing(8);

    auto* title = new QLabel(QString::fromUtf8("\u6307\u9488\u7f16\u8f91\u5668"), this);
    title->setStyleSheet(QStringLiteral(
        "font-size: 15px; font-weight: bold; color: #e8eaed;"));

    pathLabel_ = new QLabel(this);
    pathLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 12px;"));
    pathLabel_->setWordWrap(true);

    stateLabel_ = new QLabel(this);
    stateLabel_->setStyleSheet(QStringLiteral("color: #4dd0e1; font-size: 11px;"));

    convertHint_ = new QLabel(this);
    convertHint_->setStyleSheet(QStringLiteral("color: #ffd54f; font-size: 11px;"));
    convertHint_->setWordWrap(true);
    convertHint_->hide();

    lay->addWidget(title);
    lay->addWidget(pathLabel_);
    lay->addWidget(stateLabel_);
    lay->addWidget(convertHint_);

    auto* idGroup = new QGroupBox(QString::fromUtf8("\u6307\u9488\u6807\u8bc6"), this);
    auto* idLay = new QFormLayout(idGroup);
    shaEdit_ = new QLineEdit(idGroup);
    shaEdit_->setReadOnly(true);
    shaEdit_->setFont(QFont(QStringLiteral("Consolas"), 9));
    idLay->addRow(QString::fromUtf8("SHA-256:"), shaEdit_);
    lay->addWidget(idGroup);

    auto* namesGroup = new QGroupBox(QString::fromUtf8("\u539f\u59cb\u6587\u4ef6\u540d (\u4e00\u884c\u4e00\u4e2a)"), this);
    auto* namesLay = new QVBoxLayout(namesGroup);
    namesEdit_ = new QTextEdit(namesGroup);
    namesEdit_->setMaximumHeight(60);
    namesLay->addWidget(namesEdit_);
    lay->addWidget(namesGroup);

    auto* resGroup = new QGroupBox(QString::fromUtf8("\u89e3\u6790\u5668 (resolvers)"), this);
    auto* resLay = new QVBoxLayout(resGroup);

    auto* resSelRow = new QHBoxLayout;
    resolverTypeCombo_ = new QComboBox(resGroup);
    addResolverBtn_ = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u89e3\u6790\u5668"), resGroup);
    removeResolverBtn_ = new QPushButton(QString::fromUtf8("\u79fb\u9664\u6700\u540e\u4e00\u4e2a"), resGroup);
    resSelRow->addWidget(resolverTypeCombo_, 1);
    resSelRow->addWidget(addResolverBtn_);
    resSelRow->addWidget(removeResolverBtn_);
    resLay->addLayout(resSelRow);

    auto* scroll = new QScrollArea(resGroup);
    scroll->setWidgetResizable(true);
    scroll->setMaximumHeight(180);
    editorStack_ = new QStackedWidget(scroll);
    scroll->setWidget(editorStack_);
    resLay->addWidget(scroll, 1);
    lay->addWidget(resGroup);

    auto* dlGroup = new QGroupBox(QString::fromUtf8("\u4e0b\u8f7d\u65b9\u5f0f (\u6309\u4f18\u5148\u7ea7\u52fe\u9009)"), this);
    auto* dlLay = new QHBoxLayout(dlGroup);
    curlCheck_ = new QCheckBox(QStringLiteral("curl"), dlGroup);
    psCheck_ = new QCheckBox(QString::fromUtf8("PowerShell"), dlGroup);
    qtCheck_ = new QCheckBox(QString::fromUtf8("Qt Network"), dlGroup);
    qtCheck_->setChecked(true);
    dlLay->addWidget(curlCheck_);
    dlLay->addWidget(psCheck_);
    dlLay->addWidget(qtCheck_);
    dlLay->addStretch(1);
    lay->addWidget(dlGroup);

    auto* btnRow = new QHBoxLayout;
    convertButton_ = new QPushButton(QString::fromUtf8("\u8f6c\u6307\u9488"), this);
    restoreButton_ = new QPushButton(QString::fromUtf8("\u8f6c\u56de\u539f\u59cb\u6587\u4ef6"), this);
    saveButton_ = new QPushButton(QString::fromUtf8("\u4fdd\u5b58\u6307\u9488"), this);
    btnRow->addWidget(convertButton_);
    btnRow->addWidget(restoreButton_);
    btnRow->addStretch(1);
    btnRow->addWidget(saveButton_);
    lay->addLayout(btnRow);

    batchInfoLabel_ = new QLabel(this);
    batchInfoLabel_->setStyleSheet(QStringLiteral("color: #9aa0a8; font-size: 11px;"));
    batchInfoLabel_->setWordWrap(true);
    batchInfoLabel_->hide();
    lay->addWidget(batchInfoLabel_);

    lay->addStretch(1);

    loadExtensions();

    for (const auto& [type, ext] : extRegistry_) {
        resolverTypeCombo_->addItem(type, type);
        Q_UNUSED(ext);
    }
    if (resolverTypeCombo_->findText(QStringLiteral("direct_url")) < 0) {
        resolverTypeCombo_->addItem(QStringLiteral("direct_url"));
    }

    connect(addResolverBtn_, &QPushButton::clicked, this,
        &PointerEditorPanel::onAddResolver);
    connect(removeResolverBtn_, &QPushButton::clicked, this,
        &PointerEditorPanel::onRemoveResolver);
    connect(resolverTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PointerEditorPanel::onResolverTypeChanged);
    connect(saveButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onSavePointer);
    connect(restoreButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onRestoreToFile);
    connect(convertButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onConvertCurrent);
}

PointerEditorPanel::~PointerEditorPanel()
{
    if (batchThread_ && batchThread_->joinable()) {
        batchThread_->join();
    }
    unloadExtensions();
}

void PointerEditorPanel::setContext(const QString& repoDir,
    const QString& branch, const QString& branchConfigDir)
{
    repoDir_ = repoDir;
    branch_ = branch;
    branchConfigDir_ = branchConfigDir;
}

void PointerEditorPanel::loadExtensions()
{
    QStringList searchDirs;
    searchDirs << QApplication::applicationDirPath()
        + QStringLiteral("/editor/extension/pointer");
    searchDirs << QApplication::applicationDirPath()
        + QStringLiteral("/../editor/extension/pointer");
    searchDirs << QDir::currentPath()
        + QStringLiteral("/build/deploy/editor/extension/pointer");

    for (const auto& dir : searchDirs) {
        QDir extDir(dir);
        if (!extDir.exists()) continue;
        for (const auto& fi : extDir.entryInfoList({ QStringLiteral("*.dll") },
            QDir::Files)) {
            auto* lib = new QLibrary(fi.absoluteFilePath());
            if (lib->load()) {
                auto factory = reinterpret_cast<NeoCore::CreateEditorExtensionFunc>(
                    lib->resolve("CreateEditorExtension"));
                if (factory) {
                    auto* ext = factory();
                    loadedExtensions_.push_back(ext);
                    libs_.push_back(lib);
                    extRegistry_[QString::fromStdString(ext->resolverType())] = ext;
                } else {
                    lib->unload();
                    delete lib;
                }
            } else {
                delete lib;
            }
        }
    }
}

void PointerEditorPanel::unloadExtensions()
{
    clearResolverEditors();
    for (auto* lib : libs_) {
        lib->unload();
        delete lib;
    }
    libs_.clear();
    loadedExtensions_.clear();
    extRegistry_.clear();
}

void PointerEditorPanel::clearResolverEditors()
{
    while (editorStack_->count() > 0) {
        auto* w = editorStack_->widget(0);
        editorStack_->removeWidget(w);
        delete w;
    }
    resolverEditors_.clear();
}

void PointerEditorPanel::loadPointer(const QString& sha,
    const NeoCore::PointerFileData& data)
{
    convertMode_ = false;
    currentSha_ = sha;
    currentRelPath_.clear();
    currentAbsPath_.clear();

    convertButton_->hide();
    restoreButton_->show();
    saveButton_->show();
    convertHint_->hide();

    pathLabel_->setText(QString::fromUtf8("\u6307\u9488\u6587\u4ef6: branch_config/%1.pointer")
        .arg(sha.left(16) + QStringLiteral("\u2026")));
    stateLabel_->setText(QString::fromUtf8("\u2705 \u5df2\u5b58\u5728\u7684\u6307\u9488\uff0c\u53ef\u7f16\u8f91\u540e\u4fdd\u5b58\u6216\u8f6c\u56de\u539f\u59cb\u6587\u4ef6"));
    shaEdit_->setText(QString::fromStdString(data.sha256));

    QStringList names;
    for (const auto& n : data.original_names) {
        names << QString::fromStdString(n);
    }
    namesEdit_->setPlainText(names.join(QLatin1Char('\n')));

    clearResolverEditors();
    for (const auto& r : data.resolvers) {
        const QString rtype = QString::fromStdString(r.resolver);
        auto it = extRegistry_.find(rtype);
        if (it != extRegistry_.end()) {
            auto* w = it->second->createEditor(editorStack_);
            QJsonObject meta = QJsonDocument::fromJson(
                QString::fromStdString(r.metadata.dump()).toUtf8()).object();
            it->second->loadMetadata(w, meta);
            editorStack_->addWidget(w);
            resolverEditors_.push_back({ it->second, w });
        } else if (rtype == QLatin1String("direct_url")) {
            auto* w = new QWidget(editorStack_);
            auto* l = new QVBoxLayout(w);
            auto* urlEdit = new QLineEdit(w);
            urlEdit->setPlaceholderText(QString::fromUtf8("\u76f4\u94fe URL"));
            urlEdit->setText(QString::fromStdString(r.metadata.value("url", "")));
            l->addWidget(new QLabel(QString::fromUtf8("URL:"), w));
            l->addWidget(urlEdit);
            editorStack_->addWidget(w);
            resolverEditors_.push_back({ nullptr, w });
        }
    }

    curlCheck_->setChecked(false);
    psCheck_->setChecked(false);
    qtCheck_->setChecked(false);
    for (const auto& m : data.download_methods) {
        if (m == "curl") curlCheck_->setChecked(true);
        else if (m == "powershell") psCheck_->setChecked(true);
        else if (m == "qt") qtCheck_->setChecked(true);
    }
}

void PointerEditorPanel::loadFileToConvert(const QString& relPath,
    const QString& absPath)
{
    convertMode_ = true;
    currentSha_.clear();
    currentRelPath_ = relPath;
    currentAbsPath_ = absPath;

    convertButton_->show();
    restoreButton_->hide();
    saveButton_->hide();
    convertHint_->show();

    pathLabel_->setText(QString::fromUtf8("\u6587\u4ef6: %1").arg(relPath));
    stateLabel_->setText(QString::fromUtf8("\u2192 \u5c06\u8f6c\u5316\u4e3a\u6307\u9488\u5e76\u4ece\u5206\u652f\u76ee\u5f55\u79fb\u9664\u539f\u6587\u4ef6"));
    shaEdit_->clear();
    namesEdit_->setPlainText(QFileInfo(absPath).fileName());
    clearResolverEditors();
    curlCheck_->setChecked(false);
    psCheck_->setChecked(false);
    qtCheck_->setChecked(true);
}

NeoCore::PointerFileData PointerEditorPanel::currentData() const
{
    NeoCore::PointerFileData pfd;
    pfd.sha256 = shaEdit_->text().toStdString();
    const QStringList names = namesEdit_->toPlainText().split(
        QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const auto& n : names) {
        const QString trimmed = n.trimmed();
        if (!trimmed.isEmpty()) {
            pfd.original_names.push_back(trimmed.toStdString());
        }
    }
    for (const auto& re : resolverEditors_) {
        NeoCore::PointerInfo pi;
        pi.sha256 = pfd.sha256;
        if (re.extension) {
            pi.resolver = re.extension->resolverType();
            QJsonObject meta = re.extension->saveMetadata(re.widget);
            QJsonDocument doc(meta);
            pi.metadata = nlohmann::json::parse(doc.toJson().toStdString());
        } else {
            pi.resolver = "direct_url";
            auto* urlEdit = re.widget->findChild<QLineEdit*>();
            pi.metadata["url"] = urlEdit
                ? urlEdit->text().toStdString() : "";
        }
        pfd.resolvers.push_back(pi);
    }
    if (curlCheck_->isChecked()) pfd.download_methods.push_back("curl");
    if (psCheck_->isChecked()) pfd.download_methods.push_back("powershell");
    if (qtCheck_->isChecked()) pfd.download_methods.push_back("qt");
    return pfd;
}

std::string PointerEditorPanel::branchConfigPath() const
{
    return branchConfigDir_.toStdString() + "/" + branch_.toStdString() + ".json";
}

bool PointerEditorPanel::updateBranchConfig(const std::string& bcDir,
    const std::string& branch, const std::string& relPath,
    const std::string& sha, const NeoCore::PointerInfo& info,
    bool removePointer)
{
    const std::string filePath = bcDir + "/" + branch + ".json";
    nlohmann::json j;
    {
        std::ifstream f(filePath);
        if (f.is_open()) {
            try {
                j = nlohmann::json::parse(f);
            } catch (...) {}
        }
    }
    if (!j.is_object()) {
        j["file_manifest"] = nlohmann::json::object();
        j["pointer_files"] = nlohmann::json::object();
    }
    if (!j.contains("file_manifest") || !j["file_manifest"].is_object()) {
        j["file_manifest"] = nlohmann::json::object();
    }
    if (!j.contains("pointer_files") || !j["pointer_files"].is_object()) {
        j["pointer_files"] = nlohmann::json::object();
    }

    if (removePointer) {
        j["pointer_files"].erase(sha);
        j["file_manifest"].erase(relPath);
    } else {
        j["file_manifest"][relPath] = sha;
        nlohmann::json entry;
        entry["resolver"] = info.resolver;
        entry["metadata"] = info.metadata;
        j["pointer_files"][sha] = entry;
    }

    QDir().mkpath(QString::fromStdString(bcDir));
    std::ofstream f(filePath);
    if (!f.is_open()) return false;
    f << j.dump(2) << std::endl;
    f.close();
    return true;
}

void PointerEditorPanel::onAddResolver()
{
    if (static_cast<int>(resolverEditors_.size()) >= MaxResolvers) {
        QMessageBox::information(this, QString::fromUtf8("\u5df2\u6ee1"),
            QString::fromUtf8("\u6700\u591a\u652f\u6301 %1 \u4e2a\u89e3\u6790\u5668\u6761\u76ee\u3002")
                .arg(MaxResolvers));
        return;
    }
    onResolverTypeChanged();
}

void PointerEditorPanel::onRemoveResolver()
{
    if (resolverEditors_.empty()) return;
    auto* last = resolverEditors_.back().widget;
    editorStack_->removeWidget(last);
    delete last;
    resolverEditors_.pop_back();
    if (editorStack_->count() > 0) {
        editorStack_->setCurrentIndex(editorStack_->count() - 1);
    }
}

void PointerEditorPanel::onResolverTypeChanged()
{
    const QString type = resolverTypeCombo_->currentText();
    auto it = extRegistry_.find(type);
    QWidget* w = nullptr;
    if (it != extRegistry_.end()) {
        w = it->second->createEditor(editorStack_);
        editorStack_->addWidget(w);
        resolverEditors_.push_back({ it->second, w });
    } else if (type == QLatin1String("direct_url")) {
        w = new QWidget(editorStack_);
        auto* l = new QVBoxLayout(w);
        auto* urlEdit = new QLineEdit(w);
        urlEdit->setPlaceholderText(QString::fromUtf8("\u76f4\u94fe URL"));
        l->addWidget(new QLabel(QString::fromUtf8("URL:"), w));
        l->addWidget(urlEdit);
        editorStack_->addWidget(w);
        resolverEditors_.push_back({ nullptr, w });
    }
    if (w) {
        editorStack_->setCurrentWidget(w);
    }
}

void PointerEditorPanel::onSavePointer()
{
    const std::string sha = shaEdit_->text().toStdString();
    if (sha.length() != 64 || currentSha_.isEmpty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
            QString::fromUtf8("SHA-256 \u4e0d\u6709\u6548\u6216\u65e0\u6cd5\u786e\u5b9a\u6307\u9488\u76ee\u6807\u3002"));
        return;
    }
    auto pfd = currentData();
    if (pfd.resolvers.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
            QString::fromUtf8("\u81f3\u5c11\u9700\u8981\u4e00\u4e2a\u89e3\u6790\u5668\u3002"));
        return;
    }
    QDir().mkpath(branchConfigDir_);
    const std::string filePath = branchConfigDir_.toStdString()
        + "/" + sha + ".pointer";
    {
        std::ofstream f(filePath);
        if (!f.is_open()) {
            QMessageBox::warning(this, QString::fromUtf8("\u4fdd\u5b58\u5931\u8d25"),
                QString::fromUtf8("\u65e0\u6cd5\u5199\u5165\u6307\u9488\u6587\u4ef6\u3002"));
            return;
        }
        f << pfd.toJson().dump(2) << std::endl;
        f.close();
    }
    emit pointerSaved(QString::fromStdString(sha));
    emit requestRefresh();
    appendLog(QString::fromUtf8("\u6307\u9488\u5df2\u4fdd\u5b58: %1.pointer")
        .arg(QString::fromStdString(sha).left(16)));
}

void PointerEditorPanel::onRestoreToFile()
{
    if (currentSha_.isEmpty()) return;
    auto pfd = currentData();
    if (pfd.resolvers.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u56de\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u89e3\u6790\u5668\uff0c\u65e0\u6cd5\u4e0b\u8f7d\u3002"));
        return;
    }
    if (pfd.original_names.empty()) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u56de\u5931\u8d25"),
            QString::fromUtf8("\u7f3a\u5c11\u539f\u59cb\u6587\u4ef6\u540d\u3002"));
        return;
    }

    const QString branchDir = repoDir_ + QStringLiteral("/branches/")
        + branch_ + QLatin1Char('/');
    const QString targetPath = branchDir + QString::fromStdString(
        pfd.original_names[0]);

    NeoBuild::PointerDownloader downloader;
    NeoCore::PointerInfo pi = pfd.resolvers[0];
    pi.sha256 = pfd.sha256;

    const std::string cacheDir = (branchConfigDir_
        + QStringLiteral("/.download-cache")).toStdString();
    QDir().mkpath(QString::fromStdString(cacheDir));

    appendLog(QString::fromUtf8("\u6b63\u5728\u4e0b\u8f7d %1 \u2026")
        .arg(QString::fromStdString(pfd.original_names[0])));

    const auto result = downloader.download(pi, cacheDir);
    if (!result.success || result.sha256 != pfd.sha256) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u56de\u5931\u8d25"),
            QString::fromUtf8("SHA-256 \u6821\u9a8c\u5931\u8d25\u6216\u4e0b\u8f7d\u5931\u8d25:\n%1")
                .arg(QString::fromStdString(result.errorMessage.empty()
                    ? (result.sha256 == pfd.sha256
                        ? std::string("unknown error")
                        : "sha256 mismatch")
                    : result.errorMessage)));
        return;
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    if (!QFile::remove(targetPath) && QFile::exists(targetPath)) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u56de\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u8986\u76d6\u76ee\u6807\u6587\u4ef6:\n%1").arg(targetPath));
        return;
    }
    QFile::copy(QString::fromStdString(result.cachedPath), targetPath);

    QFile::remove(QString::fromStdString(branchConfigDir_.toStdString()
        + "/" + currentSha_.toStdString() + ".pointer"));

    updateBranchConfig(branchConfigDir_.toStdString(), branch_.toStdString(),
        pfd.original_names[0], currentSha_.toStdString(), pi, true);

    emit gitAddRequested({ targetPath,
        QString::fromStdString(branchConfigPath()) });
    emit branchConfigChanged(branch_);
    emit requestRefresh();
    appendLog(QString::fromUtf8("\u2705 \u5df2\u8f6c\u56de: %1")
        .arg(QString::fromStdString(pfd.original_names[0])));
}

void PointerEditorPanel::onConvertCurrent()
{
    if (convertMode_ && currentAbsPath_.isEmpty()) return;
    if (!QFile::exists(currentAbsPath_)) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
            QString::fromUtf8("\u6e90\u6587\u4ef6\u4e0d\u5b58\u5728:\n%1").arg(currentAbsPath_));
        return;
    }

    const QString type = resolverTypeCombo_->currentText();
    std::string sha = computeFileSha256(currentAbsPath_);
    if (sha.length() != 64) {
        QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
            QString::fromUtf8("\u65e0\u6cd5\u8ba1\u7b97 SHA-256\u3002"));
        return;
    }

    NeoCore::PointerInfo pi;
    pi.sha256 = sha;
    if (type == QLatin1String("modrinth")) {
        QStringList names = namesEdit_->toPlainText().split(
            QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (names.isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
                QString::fromUtf8("\u8bf7\u5728\u539f\u59cb\u6587\u4ef6\u540d\u5904\u586b\u5165 modId\u3002"));
            return;
        }
        std::string pid, vid;
        const std::string modId = names[0].toStdString();
        appendLog(QString::fromUtf8("\u67e5\u8be2 Modrinth: %1 \u2026")
            .arg(QString::fromStdString(modId)));
        if (!searchModrinth(modId, pid, vid)) {
            QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
                QString::fromUtf8("Modrinth \u67e5\u8be2\u5931\u8d25 (\u65e0\u6cd5\u627e\u5230\u8be5 modId)\u3002"));
            return;
        }
        pi.resolver = "modrinth";
        pi.metadata["project_id"] = pid;
        pi.metadata["version_id"] = vid;
    } else {
        auto* urlEdit = editorStack_->count() > 0
            ? editorStack_->currentWidget()->findChild<QLineEdit*>()
            : nullptr;
        const QString url = urlEdit ? urlEdit->text().trimmed() : QString();
        if (url.isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
                QString::fromUtf8("\u8bf7\u8f93\u5165\u76f4\u94fe URL\u3002"));
            return;
        }
        pi.resolver = "direct_url";
        pi.metadata["url"] = url.toStdString();
    }

    NeoCore::PointerFileData pfd;
    pfd.sha256 = sha;
    pfd.original_names.push_back(
        QFileInfo(currentAbsPath_).fileName().toStdString());
    pfd.resolvers.push_back(pi);
    pfd.download_methods.push_back("qt");

    QDir().mkpath(branchConfigDir_);
    {
        std::ofstream f(branchConfigDir_.toStdString() + "/" + sha + ".pointer");
        f << pfd.toJson().dump(2) << std::endl;
        f.close();
    }
    updateBranchConfig(branchConfigDir_.toStdString(), branch_.toStdString(),
        currentRelPath_.toStdString(), sha, pi, false);

    QFile::remove(currentAbsPath_);

    emit pointerSaved(QString::fromStdString(sha));
    emit gitAddRequested({ QString::fromStdString(branchConfigDir_.toStdString()
        + "/" + sha + ".pointer"),
        QString::fromStdString(branchConfigPath()) });
    emit branchConfigChanged(branch_);
    emit requestRefresh();
    appendLog(QString::fromUtf8("\u2705 \u5df2\u8f6c\u6307\u9488: %1 -> %2.pointer")
        .arg(currentRelPath_,
            QString::fromStdString(sha).left(16)));
}

void PointerEditorPanel::batchConvertJars(const QString& folderPath)
{
    doBatchConvert(folderPath);
}

void PointerEditorPanel::doBatchConvert(const QString& folderPath)
{
    QDir jd(folderPath);
    const QStringList jars = jd.entryList({ QStringLiteral("*.jar") },
        QDir::Files, QDir::Name);
    if (jars.isEmpty()) {
        appendLog(QString::fromUtf8("\u8be5\u6587\u4ef6\u5939\u4e0d\u542b JAR\u3002"));
        return;
    }

    appendLog(QString::fromUtf8("\u5f00\u59cb\u6279\u91cf\u8f6c\u6362 %1 \u4e2a JAR \u2026")
        .arg(jars.size()));
    batchInfoLabel_->show();

    const auto stats = std::make_shared<BatchStats>();
    const int total = jars.size();

    QPointer<PointerEditorPanel> guard = this;
    const QString bcDir = branchConfigDir_;
    const QString branchName = branch_;
    const QString repoDir = repoDir_;
    const QString branchRoot = repoDir + QStringLiteral("/branches/") + branchName;
    const QString cacheRoot = repoDir + QStringLiteral("/.NSUM/pointer-cache");

    if (batchThread_ && batchThread_->joinable()) {
        batchThread_->join();
    }
    batchThread_ = std::make_unique<std::thread>(
        [guard, stats, total, jars, folderPath, bcDir, branchName,
         branchRoot, cacheRoot]() {
            for (int i = 0; i < jars.size(); ++i) {
                const QString jarName = jars[i];
                const std::string fp = (folderPath + QLatin1Char('/') + jarName)
                    .toStdString();

                const std::string modId = extractModIdFromJar(fp);
                const std::string sha = computeFileSha256(
                    folderPath + QLatin1Char('/') + jarName);
                std::string pid, vid;
                bool ok = false;
                if (!modId.empty() && !sha.empty()
                    && searchModrinth(modId, pid, vid)) {
                    NeoCore::PointerFileData pfd;
                    pfd.sha256 = sha;
                    pfd.original_names.push_back(jarName.toStdString());
                    NeoCore::PointerInfo pi;
                    pi.sha256 = sha;
                    pi.resolver = "modrinth";
                    pi.metadata["project_id"] = pid;
                    pi.metadata["version_id"] = vid;
                    pfd.resolvers.push_back(pi);
                    pfd.download_methods.push_back("qt");

                    std::ofstream f(bcDir.toStdString() + "/" + sha
                        + ".pointer");
                    f << pfd.toJson().dump(2) << std::endl;
                    f.close();

                    const QString relPath = QDir(branchRoot)
                        .relativeFilePath(folderPath + QLatin1Char('/') + jarName);
                    updateBranchConfig(bcDir.toStdString(),
                        branchName.toStdString(), relPath.toStdString(),
                        sha, pi, false);

                    const QString cacheAbs = cacheRoot + QLatin1Char('/') + relPath;
                    QDir().mkpath(QFileInfo(cacheAbs).absolutePath());
                    QFile::remove(cacheAbs);
                    QFile::rename(folderPath + QLatin1Char('/') + jarName,
                        cacheAbs);
                    stats->converted.push_back(
                        { QString::fromStdString(sha), relPath, cacheAbs,
                          QString::fromStdString(pfd.toJson().dump(2)) });
                    ok = true;
                }
                stats->done.fetch_add(1);
                if (ok) stats->success.fetch_add(1);
                else stats->failed.fetch_add(1);

                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(guard.data(), [guard, stats, total, jarName, ok]() {
                        if (guard.isNull()) return;
                        guard->batchInfoLabel_->setText(
                            QString::fromUtf8("\u6b63\u5728\u5904\u7406 %2/%3 %1 \u2026")
                                .arg(jarName)
                                .arg(stats->done.load())
                                .arg(total));
                        if (!ok) {
                            guard->appendLog(
                                QString::fromUtf8("\u2718 \u5931\u8d25: %1")
                                    .arg(jarName));
                        }
                    }, Qt::QueuedConnection);
                }
            }

            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(), [guard, stats]() {
                    if (guard.isNull()) return;
                    guard->batchInfoLabel_->setText(
                        QString::fromUtf8(
                            "\u6279\u91cf\u8f6c\u6362\u5b8c\u6210\uff1a\u6210\u529f %1\uff0c\u5931\u8d25 %2\u3002")
                            .arg(stats->success.load())
                            .arg(stats->failed.load()));
                    guard->emit batchConvertFinished(
                        stats->converted, stats->failed.load());
                    guard->emit requestRefresh();
                }, Qt::QueuedConnection);
            }
        });
}

void PointerEditorPanel::appendLog(const QString& line)
{
    emit logMessage(line);
    CLogger::Info("PointerEditor: {}", line.toStdString());
}

void PointerEditorPanel::removeConverted(const QVector<ConvertedItem>& items)
{
    const QString bcDir = branchConfigDir_;
    const QString branchName = branch_;
    const QString branchRoot = repoDir_ + QStringLiteral("/branches/") + branchName;

    for (const ConvertedItem& item : items) {
        const QString targetAbs = branchRoot + QLatin1Char('/') + item.relPath;
        QDir().mkpath(QFileInfo(targetAbs).absolutePath());
        QFile::remove(targetAbs);
        QFile::rename(item.cacheAbs, targetAbs);

        const std::string pointerPath = bcDir.toStdString() + "/"
            + item.sha.toStdString() + ".pointer";
        QFile::remove(QString::fromStdString(pointerPath));

        NeoCore::PointerInfo pi;
        pi.sha256 = item.sha.toStdString();
        updateBranchConfig(bcDir.toStdString(), branchName.toStdString(),
            item.relPath.toStdString(), item.sha.toStdString(), pi, true);
    }

    if (!items.isEmpty()) {
        emit gitAddRequested({ branchConfigDir_ + QStringLiteral("/")
            + branch_ + QStringLiteral(".json") });
        emit branchConfigChanged(branch_);
        emit requestRefresh();
    }
}

void PointerEditorPanel::undoBatchConvert(const QVector<ConvertedItem>& items)
{
    removeConverted(items);
}

void PointerEditorPanel::redoBatchConvert(const QVector<ConvertedItem>& items)
{
    const QString bcDir = branchConfigDir_;
    const QString branchName = branch_;
    const QString branchRoot = repoDir_ + QStringLiteral("/branches/") + branchName;

    for (const ConvertedItem& item : items) {
        const QString sourceAbs = branchRoot + QLatin1Char('/') + item.relPath;
        if (!QFile::exists(sourceAbs)) continue;

        NeoCore::PointerFileData pfd;
        if (!item.pointerJson.isEmpty()) {
            try {
                const auto j = nlohmann::json::parse(item.pointerJson.toStdString());
                pfd = NeoCore::PointerFileData::fromJson(j);
            } catch (...) {}
        }
        if (pfd.resolvers.empty()) continue;
        const NeoCore::PointerInfo& pi = pfd.resolvers.front();

        const std::string pointerPath = bcDir.toStdString() + "/"
            + item.sha.toStdString() + ".pointer";
        QDir().mkpath(bcDir);
        {
            std::ofstream f(pointerPath);
            f << item.pointerJson.toStdString() << std::endl;
            f.close();
        }

        updateBranchConfig(bcDir.toStdString(), branchName.toStdString(),
            item.relPath.toStdString(), item.sha.toStdString(), pi, false);

        QDir().mkpath(QFileInfo(item.cacheAbs).absolutePath());
        QFile::remove(item.cacheAbs);
        QFile::rename(sourceAbs, item.cacheAbs);
    }

    if (!items.isEmpty()) {
        emit gitAddRequested({ branchConfigDir_ + QStringLiteral("/")
            + branch_ + QStringLiteral(".json") });
        emit branchConfigChanged(branch_);
        emit requestRefresh();
    }
}

bool PointerEditorPanel::restorePointerFromCache(const QString& sha,
    const NeoCore::PointerFileData& data)
{
    if (repoDir_.isEmpty() || branch_.isEmpty() || sha.isEmpty()) return false;

    const QString branchRoot = repoDir_ + QStringLiteral("/branches/")
        + branch_;
    const QString cacheRoot = repoDir_ + QStringLiteral("/.NSUM/pointer-cache");
    const QString bcDir = branchConfigDir_;

    const std::string filePath = bcDir.toStdString() + "/"
        + branch_.toStdString() + ".json";
    nlohmann::json j;
    {
        std::ifstream f(filePath);
        if (f.is_open()) {
            try {
                j = nlohmann::json::parse(f);
            } catch (...) {}
        }
    }

    QStringList relPaths;
    if (j.is_object() && j.contains("file_manifest")
        && j["file_manifest"].is_object()) {
        for (auto it = j["file_manifest"].begin();
            it != j["file_manifest"].end(); ++it) {
            if (it.value().is_string()
                && it.value().get<std::string>() == sha.toStdString()) {
                relPaths << QString::fromStdString(it.key());
            }
        }
    }
    if (relPaths.isEmpty()) {
        if (!data.original_names.empty()) {
            relPaths << QString::fromStdString(data.original_names[0]);
        }
    }
    if (relPaths.isEmpty()) return false;

    QString restored;
    for (const QString& rel : relPaths) {
        const QString cacheAbs = cacheRoot + QLatin1Char('/') + rel;
        if (!QFile::exists(cacheAbs)) continue;
        const std::string cachedSha = computeFileSha256(cacheAbs);
        if (cachedSha != sha.toStdString()) {
            appendLog(QString::fromUtf8("\u2718 SHA-256 \u4e0d\u5339\u914d: %1")
                .arg(rel));
            continue;
        }
        const QString targetAbs = branchRoot + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(targetAbs).absolutePath());
        QFile::remove(targetAbs);
        if (!QFile::copy(cacheAbs, targetAbs)) {
            appendLog(QString::fromUtf8("\u2718 \u5199\u56de\u5931\u8d25: %1").arg(rel));
            continue;
        }
        const std::string pointerPath = bcDir.toStdString() + "/"
            + sha.toStdString() + ".pointer";
        QFile::remove(QString::fromStdString(pointerPath));

        NeoCore::PointerInfo pi;
        pi.sha256 = sha.toStdString();
        updateBranchConfig(bcDir.toStdString(), branch_.toStdString(),
            rel.toStdString(), sha.toStdString(), pi, true);
        restored = rel;
        break;
    }

    if (restored.isEmpty()) return false;

    emit gitAddRequested({ branchConfigDir_ + QStringLiteral("/")
        + branch_ + QStringLiteral(".json"),
        branchRoot + QLatin1Char('/') + restored });
    emit branchConfigChanged(branch_);
    emit requestRefresh();
    return true;
}

} // namespace GUIWorker

#include "pointer_editor_panel.moc"
