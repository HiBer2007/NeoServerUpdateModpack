#include "pointer_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QSplitter>
#include <QMessageBox>
#include <QFileDialog>
#include <QProgressDialog>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QTimer>
#include <QLibrary>

#include <fstream>
#include <sstream>

#include <zip.h>
#include <nlohmann/json.hpp>
#include <logger.h>

namespace GUIWorker {

static QString currentSha256;
static const int MAX_RESOLVERS = 6;

PointerManager::PointerManager(QWidget* parent, const std::string& branchConfigDir)
    : QDialog(parent), ptrDir_(branchConfigDir)
{
    setWindowTitle("指针文件管理器");
    resize(940, 640);
    setMinimumSize(700, 480);
    loadExtensions();
    buildUI();
    if (!ptrDir_.empty()) refreshList();
}

PointerManager::~PointerManager()
{
    if (editorStack_) {
        while (editorStack_->count() > 0) {
            auto* w = editorStack_->widget(0);
            editorStack_->removeWidget(w);
            delete w;
        }
    }
    resolverEditors_.clear();
    for (auto* lib : libs_) {
        lib->unload();
        delete lib;
    }
    libs_.clear();
    loadedExtensions_.clear();
    extRegistry_.clear();
}

void PointerManager::setBranchConfigDir(const std::string& dir)
{
    ptrDir_ = dir;
    refreshList();
}

void PointerManager::loadExtensions()
{
    QStringList searchDirs;
    searchDirs << QApplication::applicationDirPath() + "/editor/extension/pointer";
    searchDirs << QApplication::applicationDirPath() + "/../editor/extension/pointer";
    searchDirs << QDir::currentPath() + "/build/deploy/editor/extension/pointer";

    for (auto& dir : searchDirs) {
        QDir extDir(dir);
        if (!extDir.exists()) continue;
        for (auto& fi : extDir.entryInfoList({"*.dll"}, QDir::Files)) {
            auto* lib = new QLibrary(fi.absoluteFilePath());
            if (lib->load()) {
                auto factory = reinterpret_cast<NeoCore::CreateEditorExtensionFunc>(
                    lib->resolve("CreateEditorExtension"));
                if (factory) {
                    auto* ext = factory();
                    loadedExtensions_.push_back(ext);
                    libs_.push_back(lib);
                    extRegistry_[QString::fromStdString(ext->resolverType())] = ext;
                    CLogger::Info("PointerManager: loaded extension {} ({})",
                        ext->resolverType(), fi.fileName().toStdString());
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

void PointerManager::unloadExtensions()
{
    loadedExtensions_.clear();
    extRegistry_.clear();
}

void PointerManager::buildUI()
{
    auto* outerLayout = new QVBoxLayout(this);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    auto* leftPanel = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);

    ptrList_ = new QListWidget(leftPanel);
    ptrList_->setMinimumWidth(200);
    leftLayout->addWidget(new QLabel("指针文件列表 (.pointer):", leftPanel));
    leftLayout->addWidget(ptrList_);

    newBtn_ = new QPushButton("新建指针", leftPanel);
    deleteBtn_ = new QPushButton("删除指针", leftPanel);
    batchBtn_ = new QPushButton("批量转换 JAR→指针", leftPanel);
    batchBtn_->setStyleSheet(
        "QPushButton { color: #fff; background: #007acc; padding: 6px 12px; "
        "border: none; border-radius: 4px; } QPushButton:hover { background: #0098e0; }");

    auto* leftBtnRow = new QHBoxLayout();
    leftBtnRow->addWidget(newBtn_);
    leftBtnRow->addWidget(deleteBtn_);
    leftLayout->addLayout(leftBtnRow);
    leftLayout->addWidget(batchBtn_);

    auto* rightPanel = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    auto* idGroup = new QGroupBox("指针标识", rightPanel);
    auto* idLayout = new QFormLayout(idGroup);
    sha256Edit_ = new QLineEdit(idGroup);
    sha256Edit_->setReadOnly(true);
    sha256Edit_->setPlaceholderText("SHA-256 (64位十六进制)");
    sha256Edit_->setFont(QFont("Consolas", 9));
    idLayout->addRow("SHA-256:", sha256Edit_);
    rightLayout->addWidget(idGroup);

    auto* namesGroup = new QGroupBox("原始文件名 (一行一个)", rightPanel);
    auto* namesLayout = new QVBoxLayout(namesGroup);
    namesEdit_ = new QTextEdit(namesGroup);
    namesEdit_->setMaximumHeight(64);
    namesEdit_->setPlaceholderText("sodium.jar");
    namesLayout->addWidget(namesEdit_);
    rightLayout->addWidget(namesGroup);

    resolversGroup_ = new QGroupBox("解析器", rightPanel);
    auto* resLayout = new QVBoxLayout(resolversGroup_);

    auto* resSelRow = new QHBoxLayout();
    resSelRow->addWidget(new QLabel("类型:", resolversGroup_));
    resolverTypeCombo_ = new QComboBox(resolversGroup_);
    resolverTypeCombo_->addItems({"modrinth", "direct_url", "curseforge", "github_release"});
    resSelRow->addWidget(resolverTypeCombo_);
    addResolverBtn_ = new QPushButton("添加解析器", resolversGroup_);
    removeResolverBtn_ = new QPushButton("移除", resolversGroup_);
    resSelRow->addWidget(addResolverBtn_);
    resSelRow->addWidget(removeResolverBtn_);
    resSelRow->addStretch();
    resLayout->addLayout(resSelRow);

    editorStack_ = new QStackedWidget(resolversGroup_);
    editorStack_->setMinimumHeight(120);
    resLayout->addWidget(editorStack_);
    rightLayout->addWidget(resolversGroup_);

    auto* dlGroup = new QGroupBox("下载方式 (按优先级勾选)", rightPanel);
    auto* dlLayout = new QHBoxLayout(dlGroup);
    curlCheck_ = new QCheckBox("curl", dlGroup);
    psCheck_ = new QCheckBox("PowerShell", dlGroup);
    qtCheck_ = new QCheckBox("Qt Network", dlGroup);
    qtCheck_->setChecked(true);
    dlLayout->addWidget(curlCheck_);
    dlLayout->addWidget(psCheck_);
    dlLayout->addWidget(qtCheck_);
    dlLayout->addStretch();
    rightLayout->addWidget(dlGroup);

    saveBtn_ = new QPushButton("保存指针文件", rightPanel);
    saveBtn_->setMinimumHeight(36);
    saveBtn_->setStyleSheet("QPushButton { font-weight: bold; padding: 8px; }");
    rightLayout->addWidget(saveBtn_);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);

    outerLayout->addWidget(splitter, 1);

    connect(ptrList_, &QListWidget::currentRowChanged,
        [this](int) { onSelectPointer(); });
    connect(newBtn_, &QPushButton::clicked, this, &PointerManager::onNewPointer);
    connect(deleteBtn_, &QPushButton::clicked, this, &PointerManager::onDeletePointer);
    connect(saveBtn_, &QPushButton::clicked, this, &PointerManager::onSavePointer);
    connect(batchBtn_, &QPushButton::clicked, this, &PointerManager::onBatchConvertJars);
    connect(addResolverBtn_, &QPushButton::clicked, this, &PointerManager::onAddResolver);
    connect(removeResolverBtn_, &QPushButton::clicked, this, &PointerManager::onRemoveResolver);
    connect(resolverTypeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &PointerManager::onResolverTypeChanged);
}

PointerManager::ResolverEditor PointerManager::createEditorForType(
    const QString& type, QWidget* parent)
{
    ResolverEditor re;
    auto it = extRegistry_.find(type);
    if (it != extRegistry_.end()) {
        re.extension = it->second;
        re.widget = it->second->createEditor(parent);
    }
    return re;
}

void PointerManager::refreshList()
{
    ptrList_->clear();
    cache_.clear();
    if (ptrDir_.empty()) return;
    for (auto& sha : scanPointerFiles())
        ptrList_->addItem(QString::fromStdString(sha));
}

std::vector<std::string> PointerManager::scanPointerFiles() const
{
    std::vector<std::string> result;
    QDir dir(QString::fromStdString(ptrDir_));
    if (!dir.exists()) return result;
    for (auto& fi : dir.entryInfoList({"*.pointer"}, QDir::Files)) {
        std::string name = fi.baseName().toStdString();
        if (name.length() == 64 && std::all_of(name.begin(), name.end(),
            [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
            result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void PointerManager::onResolverTypeChanged(int index)
{
    Q_UNUSED(index);
    QString type = resolverTypeCombo_->currentText();

    while (editorStack_->count() > 0) {
        auto* w = editorStack_->widget(0);
        editorStack_->removeWidget(w);
    }
    resolverEditors_.clear();

    auto re = createEditorForType(type, editorStack_);
    if (re.widget) {
        editorStack_->addWidget(re.widget);
        resolverEditors_.push_back(re);
        editorStack_->setCurrentWidget(re.widget);
    }
    modified_ = true;
}

void PointerManager::onAddResolver()
{
    if ((int)resolverEditors_.size() >= MAX_RESOLVERS) {
        QMessageBox::information(this, "已满", "最多支持 6 个解析器条目。");
        return;
    }
    if (editorStack_->count() == 0)
        onResolverTypeChanged(resolverTypeCombo_->currentIndex());
}

void PointerManager::onRemoveResolver()
{
    while (editorStack_->count() > 0) {
        auto* w = editorStack_->widget(0);
        editorStack_->removeWidget(w);
    }
    resolverEditors_.clear();
    modified_ = true;
}

void PointerManager::onSelectPointer()
{
    if (modified_ && !currentSha256.isEmpty()) {
        auto reply = QMessageBox::question(this, "未保存",
            "当前指针已修改，是否保存?", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Yes) saveCurrentPointer();
    }
    auto* item = ptrList_->currentItem();
    if (!item) { clearDetail(); return; }
    loadPointerFile(item->text().toStdString());
}

void PointerManager::loadPointerFile(const std::string& sha256)
{
    auto it = cache_.find(sha256);
    if (it != cache_.end()) {
        auto& pfd = it->second;
        sha256Edit_->setText(QString::fromStdString(pfd.sha256));
        namesEdit_->setPlainText([&]() {
            QStringList l;
            for (auto& n : pfd.original_names)
                l << QString::fromStdString(n);
            return l.join("\n");
        }());

        while (editorStack_->count() > 0)
            editorStack_->removeWidget(editorStack_->widget(0));
        resolverEditors_.clear();

        for (auto& r : pfd.resolvers) {
            QString rtype = QString::fromStdString(r.resolver);
            resolverTypeCombo_->setCurrentText(rtype);
            auto re = createEditorForType(rtype, editorStack_);
            if (re.widget && re.extension) {
                QJsonObject meta = QJsonDocument::fromJson(
                    QString::fromStdString(r.metadata.dump()).toUtf8()).object();
                re.extension->loadMetadata(re.widget, meta);
                editorStack_->addWidget(re.widget);
                resolverEditors_.push_back(re);
            }
        }
        if (editorStack_->count() > 0)
            editorStack_->setCurrentIndex(0);

        curlCheck_->setChecked(false); psCheck_->setChecked(false); qtCheck_->setChecked(false);
        for (auto& m : pfd.download_methods) {
            if (m == "curl") curlCheck_->setChecked(true);
            else if (m == "powershell") psCheck_->setChecked(true);
            else if (m == "qt") qtCheck_->setChecked(true);
        }
        currentSha256 = QString::fromStdString(sha256);
        modified_ = false;
        return;
    }

    std::string filePath = ptrDir_ + "/" + sha256 + ".pointer";
    QFileInfo fi(QString::fromStdString(filePath));
    if (!fi.exists()) { clearDetail(); return; }
    std::ifstream f(filePath);
    if (!f.is_open()) { clearDetail(); return; }
    try {
        auto j = nlohmann::json::parse(f);
        f.close();
        auto pfd = NeoCore::PointerFileData::fromJson(j);
        if (pfd.sha256.empty()) pfd.sha256 = sha256;
        cache_[sha256] = pfd;
        loadPointerFile(sha256);
    } catch (...) { f.close(); clearDetail(); }
}

void PointerManager::clearDetail()
{
    sha256Edit_->clear(); namesEdit_->clear();
    while (editorStack_->count() > 0)
        editorStack_->removeWidget(editorStack_->widget(0));
    resolverEditors_.clear();
    curlCheck_->setChecked(false); psCheck_->setChecked(false); qtCheck_->setChecked(true);
    currentSha256.clear();
    modified_ = false;
}

NeoCore::PointerFileData PointerManager::currentData() const
{
    NeoCore::PointerFileData pfd;
    pfd.sha256 = sha256Edit_->text().toStdString();
    QStringList names = namesEdit_->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (auto& n : names) {
        QString trimmed = n.trimmed();
        if (!trimmed.isEmpty()) pfd.original_names.push_back(trimmed.toStdString());
    }
    for (auto& re : resolverEditors_) {
        if (!re.extension || !re.widget) continue;
        NeoCore::PointerInfo pi;
        pi.sha256 = pfd.sha256;
        pi.resolver = re.extension->resolverType();
        QJsonObject meta = re.extension->saveMetadata(re.widget);
        QJsonDocument doc(meta);
        pi.metadata = nlohmann::json::parse(doc.toJson().toStdString());
        pfd.resolvers.push_back(pi);
    }
    if (curlCheck_->isChecked()) pfd.download_methods.push_back("curl");
    if (psCheck_->isChecked()) pfd.download_methods.push_back("powershell");
    if (qtCheck_->isChecked()) pfd.download_methods.push_back("qt");
    return pfd;
}

void PointerManager::onNewPointer()
{
    if (modified_) {
        auto reply = QMessageBox::question(this, "未保存",
            "当前指针已修改，是否先保存?", QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::Yes) saveCurrentPointer();
    }
    clearDetail();
    QString sha = QString(64, '0');
    sha256Edit_->setText(sha);
    currentSha256 = sha;
    namesEdit_->setFocus();
    modified_ = true;
}

void PointerManager::onDeletePointer()
{
    std::string sha = currentSha256.toStdString();
    if (sha.empty()) return;
    auto reply = QMessageBox::warning(this, "确认删除",
        QString("确定要删除指针文件 %1.pointer 吗?").arg(QString::fromStdString(sha)),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;
    QFile::remove(QString::fromStdString(ptrDir_ + "/" + sha + ".pointer"));
    cache_.erase(sha);
    refreshList();
    clearDetail();
}

bool PointerManager::saveCurrentPointer()
{
    std::string sha = currentSha256.toStdString();
    if (sha.empty()) return false;
    auto pfd = currentData();
    if (pfd.sha256.empty()) return false;
    cache_[sha] = pfd;
    if (!ptrDir_.empty()) {
        QDir().mkpath(QString::fromStdString(ptrDir_));
        std::ofstream f(ptrDir_ + "/" + sha + ".pointer");
        f << pfd.toJson().dump(2) << std::endl;
    }
    modified_ = false;
    refreshList();
    return true;
}

void PointerManager::onSavePointer()
{
    if (saveCurrentPointer())
        QMessageBox::information(this, "保存成功", "指针文件已保存。");
}

void PointerManager::onCurrentChanged() { modified_ = true; }

static std::string computeFileSha256(const QString& fp)
{
    QFile f(fp); if (!f.open(QIODevice::ReadOnly)) return "";
    QCryptographicHash h(QCryptographicHash::Sha256);
    h.addData(&f); f.close();
    return h.result().toHex().toStdString();
}

static std::string extractModIdFromJar(const std::string& jarPath)
{
    int err = 0; zip_t* a = zip_open(jarPath.c_str(), ZIP_RDONLY, &err);
    if (!a) return "";
    auto read = [&](const char* e) -> std::string {
        zip_stat_t s; if (zip_stat(a, e, 0, &s) != 0) return "";
        zip_file_t* zf = zip_fopen(a, e, 0); if (!zf) return "";
        std::vector<char> buf(s.size + 1);
        zip_fread(zf, buf.data(), s.size); buf[s.size] = 0; zip_fclose(zf);
        return std::string(buf.data());
    };
    std::string raw = read("fabric.mod.json");
    if (!raw.empty()) { try { auto j = nlohmann::json::parse(raw); zip_close(a); return j.value("id", ""); } catch (...) {} }
    raw = read("mcmod.info");
    if (!raw.empty()) { try { auto j = nlohmann::json::parse(raw); zip_close(a); if (j.is_array()&&!j.empty()) return j[0].value("modid",""); if (j.is_object()) return j.value("modid",""); } catch (...) {} }
    raw = read("META-INF/mods.toml"); if (raw.empty()) raw = read("META-INF/neoforge.mods.toml");
    if (!raw.empty()) {
        auto p = raw.find("modId"); if (p != std::string::npos) { auto eq = raw.find('=',p);
        if (eq!=std::string::npos && eq<p+30) { auto v=raw.substr(eq+1); auto nl=v.find('\n'); if(nl!=std::string::npos) v=v.substr(0,nl);
        v.erase(0,v.find_first_not_of(" \"\t\r\n")); v.erase(v.find_last_not_of(" \"\t\r\n")+1);
        zip_close(a); if(!v.empty()) return v; } }
    }
    zip_close(a); return "";
}

static bool searchModrinth(const std::string& modId, std::string& oPid, std::string& oVid)
{
    QUrl url(QString::fromStdString("https://api.modrinth.com/v2/project/" + modId + "/version"));
    QNetworkRequest req(url); req.setRawHeader("User-Agent","NeoServerUpdateModpack/1.0"); req.setTransferTimeout(15000);
    QNetworkAccessManager mgr; QNetworkReply* r = mgr.get(req);
    QEventLoop loop; QTimer::singleShot(15000,&loop,&QEventLoop::quit);
    QObject::connect(r,&QNetworkReply::finished,&loop,&QEventLoop::quit); loop.exec();
    if (r->error()!=QNetworkReply::NoError) { r->deleteLater(); return false; }
    QByteArray d = r->readAll(); r->deleteLater();
    try { auto arr = nlohmann::json::parse(d.toStdString()); if(!arr.is_array()||arr.empty()) return false;
    oPid=modId; oVid=arr[0].value("id",""); return !oVid.empty(); } catch(...) {}
    return false;
}

void PointerManager::onBatchConvertJars()
{
    QString dir = QFileDialog::getExistingDirectory(this,"选择包含 JAR 文件的目录");
    if (dir.isEmpty()) return;
    QDir jd(dir); auto jars = jd.entryList({"*.jar"},QDir::Files);
    if (jars.isEmpty()) { QMessageBox::information(this,"无文件","所选目录中没有 JAR 文件。"); return; }
    QProgressDialog progress("正在解析 JAR 并创建指针文件...","取消",0,jars.size(),this);
    progress.setWindowModality(Qt::WindowModal);
    int converted=0,failed=0;
    for (int i=0;i<jars.size();++i) {
        if(progress.wasCanceled()) break;
        progress.setValue(i); progress.setLabelText(QString("处理: %1 (%2/%3)").arg(jars[i]).arg(i+1).arg(jars.size()));
        QApplication::processEvents();
        std::string fp = (dir+"/"+jars[i]).toStdString();
        std::string modId = extractModIdFromJar(fp);
        if(modId.empty()){failed++;continue;}
        std::string sha = computeFileSha256(dir+"/"+jars[i]);
        if(sha.empty()){failed++;continue;}
        std::string pid,vid; if(!searchModrinth(modId,pid,vid)){failed++;continue;}
        NeoCore::PointerFileData pfd; pfd.sha256=sha; pfd.original_names.push_back(jars[i].toStdString());
        NeoCore::PointerInfo pi; pi.sha256=sha; pi.resolver="modrinth";
        pi.metadata["project_id"]=pid; pi.metadata["version_id"]=vid;
        pfd.resolvers.push_back(pi); pfd.download_methods.push_back("qt");
        std::ofstream f(ptrDir_+"/"+sha+".pointer"); f<<pfd.toJson().dump(2)<<std::endl; f.close();
        converted++;
    }
    progress.setValue(jars.size()); refreshList();
    QMessageBox::information(this,"批量转换完成",QString("成功: %1\n失败: %2").arg(converted).arg(failed));
}

} // namespace GUIWorker
