#include "pointer_editor_panel.h"

#include "editor_extension_registry.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <QFormLayout>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QJsonDocument>
#include <QTimer>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QEventLoop>
#include <QTimer>
#include <QFont>
#include <QScrollArea>
#include <QResizeEvent>
#include <cctype>

#include <progress_card.h>
#include <cancel_token.h>

#include <fstream>
#include <thread>
#include <atomic>
#include <unordered_map>

#include <zip.h>
#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

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

std::string computeFileSha1(const QString& fp)
{
    QFile f(fp);
    if (!f.open(QIODevice::ReadOnly)) return "";
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(&f);
    f.close();
    return h.result().toHex().toStdString();
}

struct ModJarMeta {
    std::string id;
    std::string name;
    std::string version;
};

// 按 参考/解析模组元数据.MD: fabric.mod.json → mcmod.info → mods.toml/neoforge.mods.toml
ModJarMeta extractJarMeta(const std::string& jarPath)
{
    ModJarMeta meta;
    int err = 0;
    zip_t* a = zip_open(jarPath.c_str(), ZIP_RDONLY, &err);
    if (!a) return meta;
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
    auto pick = [&](const std::string& raw, const char* idKey,
        const char* nameKey, const char* verKey) {
        try {
            auto j = nlohmann::json::parse(raw);
            if (meta.id.empty()) meta.id = j.value(idKey, "");
            if (meta.name.empty()) meta.name = j.value(nameKey, "");
            if (meta.version.empty()) meta.version = j.value(verKey, "");
        } catch (...) {}
    };
    std::string raw = read("fabric.mod.json");
    if (!raw.empty()) {
        pick(raw, "id", "name", "version");
        if (!meta.id.empty()) {
            zip_close(a);
            return meta;
        }
    }
    raw = read("mcmod.info");
    if (!raw.empty()) {
        try {
            auto j = nlohmann::json::parse(raw);
            if (j.is_array() && !j.empty()) {
                const auto& m = j[0];
                if (meta.id.empty()) meta.id = m.value("modid", "");
                if (meta.name.empty()) meta.name = m.value("name", "");
                if (meta.version.empty()) meta.version = m.value("version", "");
            } else if (j.is_object()) {
                if (meta.id.empty()) meta.id = j.value("modid", "");
                if (meta.name.empty()) meta.name = j.value("name", "");
                if (meta.version.empty()) meta.version = j.value("version", "");
            }
        } catch (...) {}
        if (!meta.id.empty()) {
            zip_close(a);
            return meta;
        }
    }
    raw = read("META-INF/mods.toml");
    if (raw.empty()) raw = read("META-INF/neoforge.mods.toml");
    if (!raw.empty()) {
        // 手写 find 解析遇行内注释/依赖块会串数据 (2026-08-20 实测
        // modId 后拼入 "# mandatory" 等), 改用 toml++ 完整解析 mods 数组
        try {
            const auto tbl = toml::parse(raw);
            if (const auto* modsArr = tbl["mods"].as_array()) {
                if (const auto* first = modsArr->get(0)) {
                    if (meta.id.empty()) {
                        const auto* v = first->as_table()->get("modId");
                        if (v && v->is_string()) meta.id = v->value_or("");
                    }
                    if (meta.name.empty()) {
                        const auto* v = first->as_table()->get("displayName");
                        if (v && v->is_string()) meta.name = v->value_or("");
                    }
                    if (meta.version.empty()) {
                        const auto* v = first->as_table()->get("version");
                        if (v && v->is_string()) meta.version = v->value_or("");
                    }
                }
            }
        } catch (const std::exception&) {
            // 解析失败保持 meta 为空, 由调用方报 "no mod id found in jar"
        }
        if (!meta.id.empty()) {
            zip_close(a);
            return meta;
        }
    }
    zip_close(a);
    return meta;
}

// Modrinth 匹配键规范化: 小写 + 忽略 -_ 与空格 (modId 与 slug 差异消除)
std::string normModKey(std::string s)
{
    std::string out;
    for (char c : s) {
        if (c == '-' || c == '_' || c == ' ') continue;
        out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Modrinth 搜索 query 规范化: 下划线/空格 → 连字符 (实测 _ 形式搜索 0 结果)
std::string normSearchQuery(std::string s)
{
    std::string out;
    bool lastDash = false;
    for (char c : s) {
        if (c == '_' || c == ' ') {
            if (!lastDash) {
                out += '-';
                lastDash = true;
            }
            continue;
        }
        out += c;
        lastDash = false;
    }
    return out;
}

bool searchModrinth(const std::string& query,
    const std::string& gameVersion, const std::string& loader,
    std::string& oPid, std::string& oVid, std::string* errOut)
{
    auto setErr = [&](const std::string& m) {
        if (errOut) *errOut = m;
    };
    if (query.empty()) {
        setErr("empty query");
        return false;
    }

    // 1) Search API: 按 query + project_type:mod (+ 可选 game version) 搜索 project
    //    注意: 原始字符串字面量易与拼接内容混淆, 统一用转义字符串
    std::string facets = "[[\"project_type:mod\"]";
    if (!gameVersion.empty()) {
        facets += ",[\"versions:" + gameVersion + "\"]";
    }
    facets += "]";
    QUrl searchUrl(QStringLiteral("https://api.modrinth.com/v2/search"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("query"),
        QString::fromStdString(normSearchQuery(query)));
    q.addQueryItem(QStringLiteral("facets"), QString::fromStdString(facets));
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("10"));
    searchUrl.setQuery(q);

    QNetworkAccessManager mgr;
    QNetworkRequest req(searchUrl);
    req.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0");
    req.setTransferTimeout(15000);
    QNetworkReply* r = mgr.get(req);
    QEventLoop loop;
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (r->error() != QNetworkReply::NoError) {
        setErr("Modrinth search request failed: "
            + r->errorString().toStdString());
        r->deleteLater();
        return false;
    }
    const QByteArray d = r->readAll();
    r->deleteLater();

    std::string pid;
    const std::string norm = normModKey(query);
    try {
        const auto j = nlohmann::json::parse(d.toStdString());
        const auto& hits = j.value("hits", nlohmann::json::array());
        if (!hits.is_array() || hits.empty()) {
            setErr("no project matched on Modrinth");
            return false;
        }
        for (const auto& hit : hits) {
            const std::string pidCand = hit.value("project_id", "");
            if (pidCand.empty()) continue;
            const std::string slug = hit.value("slug", "");
            const std::string title = hit.value("title", "");
            if (normModKey(slug) == norm || normModKey(title) == norm) {
                pid = pidCand;
                break;
            }
        }
        if (pid.empty()) {
            setErr("no project matched on Modrinth (searched '" + query + "')");
            return false;
        }
    } catch (const std::exception& e) {
        setErr(std::string("Modrinth search parse failed: ") + e.what());
        return false;
    }

    // 2) Version API: 按 project_id 取版本, 过滤逐级降级
    //    (gv+loader) → (仅 gv) → (不过滤); 实测部分模组 loader/gv 组合为空
    auto fetchVersions = [&](const std::string& gv, const std::string& ld,
        QByteArray& out) -> bool {
        QUrl verUrl(QString::fromStdString(
            "https://api.modrinth.com/v2/project/" + pid + "/version"));
        QUrlQuery vq;
        if (!gv.empty()) {
            vq.addQueryItem(QStringLiteral("game_versions"),
                QString::fromStdString("[\"" + gv + "\"]"));
        }
        if (!ld.empty()) {
            vq.addQueryItem(QStringLiteral("loaders"),
                QString::fromStdString("[\"" + ld + "\"]"));
        }
        verUrl.setQuery(vq);
        QNetworkRequest vreq(verUrl);
        vreq.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0");
        vreq.setTransferTimeout(15000);
        QNetworkReply* vr = mgr.get(vreq);
        QEventLoop loop2;
        QTimer::singleShot(15000, &loop2, &QEventLoop::quit);
        QObject::connect(vr, &QNetworkReply::finished, &loop2, &QEventLoop::quit);
        loop2.exec();
        if (vr->error() != QNetworkReply::NoError) {
            const QString e = vr->errorString();
            vr->deleteLater();
            setErr("Modrinth version request failed: " + e.toStdString());
            return false;
        }
        out = vr->readAll();
        vr->deleteLater();
        return true;
    };

    QByteArray vd;
    bool gotVersions = false;
    if (!gameVersion.empty() || !loader.empty()) {
        gotVersions = fetchVersions(gameVersion, loader, vd);
        if (gotVersions) {
            try {
                if (nlohmann::json::parse(vd.toStdString()).empty()) {
                    gotVersions = fetchVersions(gameVersion, "", vd);
                    if (gotVersions
                        && nlohmann::json::parse(vd.toStdString()).empty()) {
                        gotVersions = fetchVersions("", "", vd);
                    }
                }
            } catch (...) {
                gotVersions = false;
            }
        }
    } else {
        gotVersions = fetchVersions("", "", vd);
    }
    if (!gotVersions) return false;

    try {
        const auto arr = nlohmann::json::parse(vd.toStdString());
        if (!arr.is_array() || arr.empty()) {
            setErr("no version found for project on Modrinth");
            return false;
        }
        oPid = pid;
        oVid = arr[0].value("id", "");
        if (oVid.empty()) {
            setErr("Modrinth version id is empty");
            return false;
        }
    } catch (const std::exception& e) {
        setErr(std::string("Modrinth version parse failed: ") + e.what());
        return false;
    }
    return true;
}

// 从仓库 workspace.json 读取游戏版本与加载器 (用于 Modrinth 搜索过滤)
void readWorkspaceGameInfo(const QString& repoDir, std::string& mc,
    std::string& loader)
{
    QFile f(repoDir + QStringLiteral("/workspace.json"));
    if (!f.open(QIODevice::ReadOnly)) return;
    try {
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        f.close();
        const auto& ws = j.value("workspace", nlohmann::json::object());
        if (ws.is_object()) {
            mc = ws.value("minecraft", "");
            loader = ws.value("modloader", "");
        }
    } catch (...) {
        f.close();
    }
}

// 批量哈希反查 (POST /v2/version_files): Modrinth 按文件哈希索引,
// 字节一致即 100% 命中, 与文件名无关。返回 {sha1: Version 对象}。
std::unordered_map<std::string, nlohmann::json> lookupVersionsByHash(
    const std::vector<std::string>& sha1s, std::string* errOut)
{
    std::unordered_map<std::string, nlohmann::json> out;
    if (sha1s.empty()) return out;

    nlohmann::json body;
    body["algorithm"] = "sha1";
    body["hashes"] = nlohmann::json::array();
    for (const auto& s : sha1s) {
        body["hashes"].push_back(s);
    }

    QNetworkAccessManager mgr;
    QNetworkRequest req(
        QUrl(QStringLiteral("https://api.modrinth.com/v2/version_files")));
    req.setRawHeader("User-Agent", "NeoServerUpdateModpack/1.0");
    req.setHeader(QNetworkRequest::ContentTypeHeader,
        QStringLiteral("application/json"));
    req.setTransferTimeout(20000);
    QNetworkReply* r = mgr.post(req,
        QByteArray::fromStdString(body.dump()));
    QEventLoop loop;
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);
    QObject::connect(r, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
    if (r->error() != QNetworkReply::NoError) {
        if (errOut) {
            *errOut = "Modrinth hash lookup failed: "
                + r->errorString().toStdString();
        }
        r->deleteLater();
        return out;
    }
    const QByteArray d = r->readAll();
    r->deleteLater();
    try {
        const auto j = nlohmann::json::parse(d.toStdString());
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.value().is_object()
                && !it.value().value("id", "").empty()) {
                out[it.key()] = it.value();
            }
        }
    } catch (const std::exception& e) {
        if (errOut) {
            *errOut = std::string("Modrinth hash lookup parse failed: ")
                + e.what();
        }
    }
    return out;
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

    idGroup_ = new QGroupBox(QString::fromUtf8("\u6307\u9488\u6807\u8bc6"), this);
    auto* idLay = new QFormLayout(idGroup_);
    shaEdit_ = new QLineEdit(idGroup_);
    shaEdit_->setReadOnly(true);
    shaEdit_->setFont(QFont(QStringLiteral("Consolas"), 9));
    idLay->addRow(QString::fromUtf8("SHA-256:"), shaEdit_);
    lay->addWidget(idGroup_);

    namesGroup_ = new QGroupBox(
        QString::fromUtf8("\u539f\u59cb\u6587\u4ef6\u540d (\u4e00\u884c\u4e00\u4e2a)"), this);
    auto* namesLay = new QVBoxLayout(namesGroup_);
    namesEdit_ = new QTextEdit(namesGroup_);
    namesEdit_->setMaximumHeight(60);
    namesLay->addWidget(namesEdit_);
    lay->addWidget(namesGroup_);

    auto* resGroup = new QGroupBox(QString::fromUtf8("\u89e3\u6790\u5668 (resolvers)"), this);
    auto* resLay = new QVBoxLayout(resGroup);

    auto* resSelRow = new QHBoxLayout;
    resolverTypeCombo_ = new QComboBox(resGroup);
    addResolverBtn_ = new QPushButton(QString::fromUtf8("\u6dfb\u52a0\u89e3\u6790\u5668"), resGroup);
    resSelRow->addWidget(new QLabel(QString::fromUtf8("\u7c7b\u578b:"), resGroup));
    resSelRow->addWidget(resolverTypeCombo_, 1);
    resSelRow->addWidget(addResolverBtn_);
    resLay->addLayout(resSelRow);

    // 解析器卡片列表 (与主程序 BranchPage 同款实现): QFrame 容器 + QFrame 卡片 + objectName 样式
    scrollArea_ = new QScrollArea(resGroup);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    resolverListHost_ = new QFrame(scrollArea_);
    resolverListHost_->setObjectName(QStringLiteral("resolverCardContainer"));
    resolverListHost_->setFrameShape(QFrame::NoFrame);
    resolverListHost_->setStyleSheet(QStringLiteral(
        "#resolverCardContainer { background: transparent; }"));
    resolverListLayout_ = new QVBoxLayout(resolverListHost_);
    resolverListLayout_->setContentsMargins(0, 0, 0, 0);
    resolverListLayout_->setSpacing(10);
    resolverListLayout_->addStretch(1);
    scrollArea_->setWidget(resolverListHost_);
    resLay->addWidget(scrollArea_, 1);
    // 解析器板块占满窗口剩余空间 (高度上限由 resizeEvent 动态计算)
    lay->addWidget(resGroup, 1);

    // 下载方式: 有序列表 (顺序 = 尝试顺序, 支持上移/下移调整)
    dlGroup_ = new QGroupBox(QString::fromUtf8("\u4e0b\u8f7d\u65b9\u5f0f"), this);
    dlGroup_->setToolTip(QString::fromUtf8(
        "\u52fe\u9009\u53c2\u4e0e\u7684\u4e0b\u8f7d\u65b9\u5f0f\uff0c\u5217\u8868\u987a\u5e8f = \u5c1d\u8bd5\u987a\u5e8f"
        "\uff08\u5148\u6210\u529f\u7684\u4f18\u5148\uff09\u3002"));
    auto* dlLay = new QHBoxLayout(dlGroup_);
    dlList_ = new QListWidget(dlGroup_);
    dlList_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    dlList_->setMaximumHeight(92);
    auto addDlItem = [&](const QString& display, const QString& key) {
        auto* it = new QListWidgetItem(display, dlList_);
        it->setData(Qt::UserRole, key);
        return it;
    };
    addDlItem(QStringLiteral("curl"), QStringLiteral("curl"));
    addDlItem(QString::fromUtf8("PowerShell"), QStringLiteral("powershell"));
    addDlItem(QString::fromUtf8("Qt Network"), QStringLiteral("qt"));
    dlList_->item(2)->setSelected(true);
    dlLay->addWidget(dlList_, 1);
    auto* dlBtnCol = new QVBoxLayout;
    dlUpBtn_ = new QPushButton(QString::fromUtf8("\u4e0a\u79fb"), dlGroup_);
    dlDownBtn_ = new QPushButton(QString::fromUtf8("\u4e0b\u79fb"), dlGroup_);
    dlBtnCol->addWidget(dlUpBtn_);
    dlBtnCol->addWidget(dlDownBtn_);
    dlBtnCol->addStretch(1);
    dlLay->addLayout(dlBtnCol);
    lay->addWidget(dlGroup_);

    auto moveDlSelected = [this](int delta) {
        QList<int> rows;
        for (auto* it : dlList_->selectedItems()) {
            rows << dlList_->row(it);
        }
        std::sort(rows.begin(), rows.end());
        if (delta < 0) {
            for (int r : rows) {
                if (r <= 0) continue;
                auto* it = dlList_->takeItem(r);
                dlList_->insertItem(r - 1, it);
                it->setSelected(true);
            }
        } else {
            for (int i = rows.size() - 1; i >= 0; --i) {
                const int r = rows[i];
                if (r >= dlList_->count() - 1) continue;
                auto* it = dlList_->takeItem(r);
                dlList_->insertItem(r + 1, it);
                it->setSelected(true);
            }
        }
    };
    connect(dlUpBtn_, &QPushButton::clicked, this,
        [moveDlSelected]() { moveDlSelected(-1); });
    connect(dlDownBtn_, &QPushButton::clicked, this,
        [moveDlSelected]() { moveDlSelected(1); });

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

    populateResolverCombo();

    connect(addResolverBtn_, &QPushButton::clicked, this,
        &PointerEditorPanel::onAddResolver);
    connect(saveButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onSavePointer);
    connect(restoreButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onRestoreToFile);
    connect(convertButton_, &QPushButton::clicked, this,
        &PointerEditorPanel::onConvertCurrent);

    // 批量转换模态卡片 (半透明遮罩 + 条形分布图 + 结果报告)
    batchCard_ = new BatchConvertCard();
    connect(batchCard_, &BatchConvertCard::cancelRequested, this, [this]() {
        if (batchCancelToken_) {
            batchCancelToken_->request_cancel();
        }
    });
}

void PointerEditorPanel::updateResolverHeight()
{
    // 解析器板块动态高度: 内容自适应, 上限 = 窗口剩余高度 (其余固定控件高度之和)
    if (!scrollArea_ || !idGroup_ || !namesGroup_ || !dlGroup_) return;
    int fixed = 26;  // 标题
    fixed += pathLabel_->sizeHint().height();
    fixed += stateLabel_->sizeHint().height();
    if (convertHint_->isVisible()) {
        fixed += convertHint_->sizeHint().height();
    }
    fixed += idGroup_->sizeHint().height();
    fixed += namesGroup_->sizeHint().height();
    fixed += dlGroup_->sizeHint().height();
    fixed += saveButton_->sizeHint().height() + 12;  // 按钮行
    if (batchInfoLabel_->isVisible()) {
        fixed += batchInfoLabel_->sizeHint().height();
    }
    fixed += 48;  // 布局边距与间距兜底
    const int avail = qMax(140, height() - fixed);
    if (scrollArea_->maximumHeight() != avail) {
        scrollArea_->setMaximumHeight(avail);
    }
}

void PointerEditorPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateResolverHeight();
}

PointerEditorPanel::~PointerEditorPanel()
{
    if (batchThread_ && batchThread_->joinable()) {
        batchThread_->join();
    }
    clearResolverEditors();
    // 模态卡片可能挂载在宿主窗口上, 交事件循环销毁 (避免析构链重入)
    if (batchCard_) {
        batchCard_->deleteLater();
        batchCard_ = nullptr;
    }
}

void PointerEditorPanel::setContext(const QString& repoDir,
    const QString& branch, const QString& branchConfigDir)
{
    repoDir_ = repoDir;
    branch_ = branch;
    branchConfigDir_ = branchConfigDir;
}

void PointerEditorPanel::setExtensionRegistry(EditorExtensionRegistry* reg)
{
    extReg_ = reg;
    clearResolverEditors();
    extRegistry_.clear();
    if (reg) {
        const auto editors = reg->pointerEditors();
        for (auto* ext : editors) {
            extRegistry_[QString::fromStdString(ext->resolverType())] = ext;
        }
    }
    populateResolverCombo();
}

void PointerEditorPanel::populateResolverCombo()
{
    resolverTypeCombo_->blockSignals(true);
    resolverTypeCombo_->clear();
    for (const auto& [type, ext] : extRegistry_) {
        resolverTypeCombo_->addItem(type, type);
        Q_UNUSED(ext);
    }
    if (resolverTypeCombo_->findText(QStringLiteral("direct_url")) < 0) {
        resolverTypeCombo_->addItem(QStringLiteral("direct_url"));
    }
    resolverTypeCombo_->blockSignals(false);
}

void PointerEditorPanel::refreshResolverArea()
{
    if (!scrollArea_ || !resolverListHost_) return;
    // 延迟到事件循环首帧: 布局懒计算, 立即 update 可能早于重排完成
    QTimer::singleShot(0, this, [this]() {
        if (!scrollArea_ || !resolverListHost_) return;
        resolverListLayout_->invalidate();
        resolverListHost_->adjustSize();
        resolverListHost_->update();
        scrollArea_->viewport()->update();
    });
}

void PointerEditorPanel::clearResolverEditors()
{
    // 清空卡片列表 (保留末尾 stretch)
    while (resolverListLayout_ && resolverListLayout_->count() > 1) {
        auto* item = resolverListLayout_->takeAt(0);
        if (auto* w = item->widget()) {
            delete w;
        }
        delete item;
    }
    resolverEditors_.clear();
    refreshResolverArea();
}

QWidget* PointerEditorPanel::addResolverCard(const QString& type)
{
    auto* card = new QFrame(resolverListHost_);
    card->setObjectName(QStringLiteral("resolverCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setStyleSheet(QStringLiteral(
        "QFrame#resolverCard { background-color: #262a30;"
        " border: 1px solid #3a4048; border-radius: 8px; }"
        "QFrame#resolverCard:hover { border: 1px solid #6ab7ff; }"));
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(12, 8, 12, 10);
    cardLay->setSpacing(6);

    auto* headRow = new QHBoxLayout;
    auto* typeLabel = new QLabel(type, card);
    typeLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; color: #6ab7ff;"));
    auto* delBtn = new QPushButton(QString::fromUtf8("\u5220\u9664"), card);
    delBtn->setFixedSize(44, 22);
    headRow->addWidget(typeLabel, 1);
    headRow->addWidget(delBtn);
    cardLay->addLayout(headRow);

    QWidget* editor = nullptr;
    auto it = extRegistry_.find(type);
    if (it != extRegistry_.end()) {
        editor = it->second->createEditor(card);
        resolverEditors_.push_back({ it->second, editor });
    } else if (type == QLatin1String("direct_url")) {
        editor = new QWidget(card);
        auto* l = new QVBoxLayout(editor);
        auto* urlEdit = new QLineEdit(editor);
        urlEdit->setPlaceholderText(QString::fromUtf8("\u76f4\u94fe URL"));
        l->addWidget(new QLabel(QString::fromUtf8("URL:"), editor));
        l->addWidget(urlEdit);
        resolverEditors_.push_back({ nullptr, editor });
    }
    if (editor) {
        cardLay->addWidget(editor);
    }

    // 删除按钮: 先从布局移除再销毁 (直接 delete 会留下悬垂布局项, 后续布局/重绘全面错乱)
    connect(delBtn, &QPushButton::clicked, this, [this, card, editor]() {
        if (editor) {
            for (auto it2 = resolverEditors_.begin();
                it2 != resolverEditors_.end(); ++it2) {
                if (it2->widget == editor) {
                    resolverEditors_.erase(it2);
                    break;
                }
            }
        }
        if (resolverListLayout_) {
            resolverListLayout_->removeWidget(card);
        }
        delete card;
        refreshResolverArea();
    });

    if (resolverListLayout_) {
        // 插到末尾 stretch 之前
        resolverListLayout_->insertWidget(
            resolverListLayout_->count() - 1, card);
    }
    refreshResolverArea();
    return editor;
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
        QWidget* w = addResolverCard(rtype);
        if (!w) continue;
        auto it = extRegistry_.find(rtype);
        if (it != extRegistry_.end()) {
            QJsonObject meta = QJsonDocument::fromJson(
                QString::fromStdString(r.metadata.dump()).toUtf8()).object();
            it->second->loadMetadata(w, meta);
        } else if (rtype == QLatin1String("direct_url")) {
            auto* urlEdit = w->findChild<QLineEdit*>();
            if (urlEdit) {
                urlEdit->setText(QString::fromStdString(
                    r.metadata.value("url", "")));
            }
        }
    }

    for (int i = 0; i < dlList_->count(); ++i) {
        dlList_->item(i)->setSelected(false);
    }
    for (const auto& m : data.download_methods) {
        const QString key = QString::fromStdString(m);
        for (int i = 0; i < dlList_->count(); ++i) {
            if (dlList_->item(i)->data(Qt::UserRole).toString() == key) {
                dlList_->item(i)->setSelected(true);
                break;
            }
        }
    }
    updateResolverHeight();
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
    for (int i = 0; i < dlList_->count(); ++i) {
        dlList_->item(i)->setSelected(
            dlList_->item(i)->data(Qt::UserRole).toString()
                == QLatin1String("qt"));
    }
    updateResolverHeight();
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
    // 下载方式按列表顺序收集 (顺序 = 尝试顺序)
    for (int i = 0; i < dlList_->count(); ++i) {
        QListWidgetItem* it = dlList_->item(i);
        if (it->isSelected()) {
            pfd.download_methods.push_back(
                it->data(Qt::UserRole).toString().toStdString());
        }
    }
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
    addResolverCard(resolverTypeCombo_->currentText());
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
        std::string gameVersion, loader;
        readWorkspaceGameInfo(repoDir_, gameVersion, loader);
        appendLog(QString::fromUtf8("\u67e5\u8be2 Modrinth: %1 \u2026")
            .arg(QString::fromStdString(modId)));
        std::string reason;
        bool found = false;
        // 优先按文件 SHA1 反查 (字节级精准), 未命中再回落 slug 搜索
        const std::string sha1 = computeFileSha1(currentAbsPath_);
        if (!sha1.empty()) {
            const auto map = lookupVersionsByHash({ sha1 }, &reason);
            const auto hit = map.find(sha1);
            if (hit != map.end()) {
                pid = hit->second.value("project_id", "");
                vid = hit->second.value("id", "");
                found = !pid.empty() && !vid.empty();
            }
        }
        if (!found) {
            if (!searchModrinth(modId, gameVersion, loader, pid, vid,
                &reason)) {
                QMessageBox::warning(this,
                    QString::fromUtf8("\u8f6c\u6307\u9488\u5931\u8d25"),
                    QString::fromUtf8("Modrinth \u67e5\u8be2\u5931\u8d25: %1")
                        .arg(QString::fromStdString(reason)));
                return;
            }
        }
        pi.resolver = "modrinth";
        pi.metadata["project_id"] = pid;
        pi.metadata["version_id"] = vid;
    } else {
        // direct_url: 取最后添加的 direct_url 卡片中的 URL
        QString url;
        for (auto it = resolverEditors_.rbegin(); it != resolverEditors_.rend(); ++it) {
            if (!it->extension) {
                auto* urlEdit = it->widget->findChild<QLineEdit*>();
                if (urlEdit) {
                    url = urlEdit->text().trimmed();
                }
                break;
            }
        }
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
    QDir jd(folderPath);
    QStringList absPaths;
    const QStringList jars = jd.entryList({ QStringLiteral("*.jar") },
        QDir::Files, QDir::Name);
    for (const QString& n : jars) {
        absPaths << folderPath + QLatin1Char('/') + n;
    }
    startBatchConvert(absPaths);
}

void PointerEditorPanel::batchConvertJarsList(const QStringList& relPaths)
{
    const QString branchRoot = repoDir_ + QStringLiteral("/branches/")
        + branch_;
    QStringList absPaths;
    for (const QString& rel : relPaths) {
        absPaths << branchRoot + QLatin1Char('/') + rel;
    }
    startBatchConvert(absPaths);
}

void PointerEditorPanel::startBatchConvert(const QStringList& absPaths)
{
    if (absPaths.isEmpty()) {
        appendLog(QString::fromUtf8("\u65e0\u53ef\u8f6c\u6362\u7684 JAR\u3002"));
        CLogger::Info("PointerEditor: no JAR files to convert");
        return;
    }

    appendLog(QString::fromUtf8("\u5f00\u59cb\u6279\u91cf\u8f6c\u6362 %1 \u4e2a JAR \u2026")
        .arg(absPaths.size()));
    CLogger::Info("PointerEditor: batch convert started: {} jar(s)",
        absPaths.size());
    batchInfoLabel_->show();
    updateResolverHeight();

    // 模态卡片: 半透明遮罩阻止其他操作 + 条形分布图 + 结果报告
    batchCancelToken_ = std::make_unique<NeoCore::CancelToken>();
    NeoCore::CancelToken* cancel = batchCancelToken_.get();
    // 卡片必须挂在编辑器主窗口内 (非独立窗口): 显式 attach 到宿主 window()
    batchCard_->attachTo(window());
    batchCard_->begin(QString::fromUtf8(
        "\u6279\u91cf\u8f6c\u6362 JAR\u2192\u6307\u9488"), absPaths.size());
    QApplication::processEvents();

    const auto stats = std::make_shared<BatchStats>();
    auto results = std::make_shared<QVector<BatchConvertResult>>();
    const int total = absPaths.size();

    // 转换周期内工作区游戏版本/加载器固定, 预先读取一次用于 Modrinth 过滤
    std::string gameVersion, loader;
    readWorkspaceGameInfo(repoDir_, gameVersion, loader);

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
        [guard, cancel, stats, results, total, absPaths, bcDir, branchName,
         branchRoot, cacheRoot, gameVersion, loader]() {
            // 阶段 1: 计算全部 jar 的 SHA1 + 解析元数据 (Modrinth 按哈希索引文件)
            struct JarWork {
                QString fp;
                QString name;
                std::string sha1;
                std::string modId;
            };
            std::vector<JarWork> works;
            works.reserve(absPaths.size());
            for (const QString& fp : absPaths) {
                const QString jarName = QFileInfo(fp).fileName();
                const ModJarMeta meta = extractJarMeta(fp.toStdString());
                works.push_back({ fp, jarName, computeFileSha1(fp),
                    meta.id });
            }

            // 阶段 2: 批量哈希反查一次完成 (未命中的再逐个回落 slug 搜索)
            std::vector<std::string> sha1s;
            sha1s.reserve(works.size());
            for (const JarWork& w : works) {
                if (!w.sha1.empty()) sha1s.push_back(w.sha1);
            }
            std::string lookupErr;
            const auto hashMap = lookupVersionsByHash(sha1s, &lookupErr);

            // 阶段 3: 逐 jar 生成指针
            for (int i = 0; i < static_cast<int>(works.size()); ++i) {
                if (cancel->is_cancelled()) break;

                const JarWork& w = works[i];
                std::string reason;
                std::string pid, vid;
                bool ok = false;

                const auto hit = hashMap.find(w.sha1);
                if (hit != hashMap.end()) {
                    pid = hit->second.value("project_id", "");
                    vid = hit->second.value("id", "");
                    ok = !pid.empty() && !vid.empty();
                    if (!ok) {
                        reason = "hash hit but version data incomplete";
                    }
                } else if (!w.sha1.empty()) {
                    reason = "file not found on Modrinth by hash";
                } else {
                    reason = "sha1 failed";
                }

                if (!ok && !w.modId.empty()) {
                    // 哈希未命中 (本地文件被魔改/非 Modrinth 来源) → 回落 slug 搜索
                    std::string searchErr;
                    if (searchModrinth(w.modId, gameVersion, loader,
                        pid, vid, &searchErr)) {
                        ok = true;
                    } else {
                        reason = searchErr.empty()
                            ? std::string("Modrinth search failed")
                            : searchErr;
                    }
                }
                if (!ok && reason.empty()) {
                    reason = "no mod id found in jar";
                }

                if (ok) {
                    const std::string sha256 = computeFileSha256(w.fp);
                    if (sha256.empty()) {
                        ok = false;
                        reason = "sha256 failed";
                    } else {
                        NeoCore::PointerFileData pfd;
                        pfd.sha256 = sha256;
                        pfd.original_names.push_back(w.name.toStdString());
                        NeoCore::PointerInfo pi;
                        pi.sha256 = sha256;
                        pi.resolver = "modrinth";
                        pi.metadata["project_id"] = pid;
                        pi.metadata["version_id"] = vid;
                        pfd.resolvers.push_back(pi);
                        pfd.download_methods.push_back("qt");

                        std::ofstream f(bcDir.toStdString() + "/" + sha256
                            + ".pointer");
                        f << pfd.toJson().dump(2) << std::endl;
                        f.close();

                        const QString relPath = QDir(branchRoot)
                            .relativeFilePath(w.fp);
                        updateBranchConfig(bcDir.toStdString(),
                            branchName.toStdString(), relPath.toStdString(),
                            sha256, pi, false);

                        const QString cacheAbs = cacheRoot + QLatin1Char('/')
                            + relPath;
                        QDir().mkpath(QFileInfo(cacheAbs).absolutePath());
                        QFile::remove(cacheAbs);
                        QFile::rename(w.fp, cacheAbs);
                        stats->converted.push_back(
                            { QString::fromStdString(sha256), relPath,
                              cacheAbs,
                              QString::fromStdString(pfd.toJson().dump(2)) });
                    }
                }
                stats->done.fetch_add(1);
                if (ok) stats->success.fetch_add(1);
                else stats->failed.fetch_add(1);
                results->push_back({ w.name, ok,
                    QString::fromStdString(reason) });

                if (!guard.isNull()) {
                    QMetaObject::invokeMethod(guard.data(),
                        [guard, stats, total, w, ok, reason]() {
                            if (guard.isNull()) return;
                            const int done = stats->done.load();
                            const int success = stats->success.load();
                            const int failed = stats->failed.load();
                            guard->batchInfoLabel_->setText(
                                QString::fromUtf8("\u6b63\u5728\u5904\u7406 %2/%3 %1 \u2026")
                                    .arg(w.name).arg(done).arg(total));
                            guard->batchCard_->setProgress(success, failed,
                                QString::fromUtf8("\u6b63\u5728\u8f6c\u6362: %1")
                                    .arg(w.name));
                            if (!ok) {
                                guard->appendLog(
                                    QString::fromUtf8("\u2718 \u5931\u8d25: %1\uff08%2\uff09")
                                        .arg(w.name,
                                            QString::fromStdString(reason)));
                                CLogger::Info(
                                    "PointerEditor: batch convert failed: {} ({})",
                                    w.name.toStdString(), reason);
                            }
                        }, Qt::QueuedConnection);
                }
            }

            if (!guard.isNull()) {
                QMetaObject::invokeMethod(guard.data(),
                    [guard, stats, cancel, total, results]() {
                    if (guard.isNull()) return;
                    const bool cancelled = cancel->is_cancelled();
                    const int done = stats->done.load();
                    const int success = stats->success.load();
                    const int failed = stats->failed.load();
                    guard->batchInfoLabel_->setText(
                        QString::fromUtf8(
                            "\u6279\u91cf\u8f6c\u6362\u5b8c\u6210\uff1a\u6210\u529f %1\uff0c\u5931\u8d25 %2\u3002")
                            .arg(success).arg(failed));
                    QString summary;
                    if (cancelled) {
                        summary = QString::fromUtf8(
                            "\u5df2\u53d6\u6d88\uff0c\u5df2\u5904\u7406 %1/%2\u3002")
                            .arg(done).arg(total);
                        CLogger::Info(
                            "PointerEditor: batch convert cancelled after {}/{}",
                            done, total);
                    } else {
                        summary = QString::fromUtf8(
                            "\u8f6c\u6362\u5b8c\u6210\uff1a\u5df2\u5b8c\u6210 %1 \uff0c\u5931\u8d25 %2\u3002"
                            "\u53ef\u5728\u4e0b\u65b9\u67e5\u770b\u8be6\u7ec6\u5217\u8868\u3002")
                            .arg(success).arg(failed);
                    }
                    guard->batchCard_->showReport(*results, summary,
                        cancelled);
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
