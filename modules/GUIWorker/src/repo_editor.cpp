#include "repo_editor.h"
#include <vector>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QHeaderView>
#include <QSplitter>

#include <logger.h>
#include <repo_urls.h>

namespace GUIWorker {

RepoEditor::RepoEditor(QWidget* parent)
    : QWidget(parent), modified_(false)
{
    buildUI();
    connectSignals();
}

void RepoEditor::buildUI()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    titleLabel_ = new QLabel("仓库设置", this);
    QFont titleFont = titleLabel_->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel_->setFont(titleFont);
    outerLayout->addWidget(titleLabel_);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto* scrollContent = new QWidget(scrollArea);
    auto* mainLayout = new QVBoxLayout(scrollContent);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(4, 4, 4, 4);

    workspaceGroup_ = new QGroupBox("工作区基本信息", scrollContent);
    auto* wsLayout = new QFormLayout(workspaceGroup_);
    nameEdit_ = new QLineEdit(workspaceGroup_);
    nameEdit_->setPlaceholderText("例如: NeoServer");
    minecraftEdit_ = new QLineEdit(workspaceGroup_);
    minecraftEdit_->setPlaceholderText("例如: 1.21.4");
    modloaderCombo_ = new QComboBox(workspaceGroup_);
    modloaderCombo_->addItems({"fabric", "forge", "neoforge", "quilt"});
    modloaderVersionEdit_ = new QLineEdit(workspaceGroup_);
    modloaderVersionEdit_->setPlaceholderText("例如: 0.16.10 (加载器版本)");
    wsLayout->addRow("整合包名称:", nameEdit_);
    wsLayout->addRow("Minecraft 版本:", minecraftEdit_);
    wsLayout->addRow("模组加载器:", modloaderCombo_);
    wsLayout->addRow("加载器版本:", modloaderVersionEdit_);

    gitGroup_ = new QGroupBox("Git 仓库", scrollContent);
    auto* gitLayout = new QFormLayout(gitGroup_);
    sshEdit_ = new QLineEdit(gitGroup_);
    sshEdit_->setPlaceholderText("git@github.com:user/repo.git");
    httpsEdit_ = new QLineEdit(gitGroup_);
    httpsEdit_->setPlaceholderText("https://github.com/user/repo.git");
    httpEdit_ = new QLineEdit(gitGroup_);
    httpEdit_->setPlaceholderText("http://github.com/user/repo.git");
    primaryCombo_ = new QComboBox(gitGroup_);
    primaryCombo_->addItem("SSH", "ssh");
    primaryCombo_->addItem("HTTPS", "https");
    primaryCombo_->addItem("HTTP", "http");
    defaultBranchEdit_ = new QLineEdit(gitGroup_);
    defaultBranchEdit_->setPlaceholderText("main");
    defaultBranchEdit_->setText("main");

    auto* testRow = new QHBoxLayout();
    testBtn_ = new QPushButton("测试连接", gitGroup_);
    testBtn_->setMaximumWidth(100);
    testResult_ = new QLabel("", gitGroup_);
    testRow->addWidget(testBtn_);
    testRow->addWidget(testResult_);
    testRow->addStretch();

    gitLayout->addRow("SSH 地址:", sshEdit_);
    gitLayout->addRow("HTTPS 地址:", httpsEdit_);
    gitLayout->addRow("HTTP 地址:", httpEdit_);
    gitLayout->addRow("主地址:", primaryCombo_);
    gitLayout->addRow("默认分支:", defaultBranchEdit_);
    gitLayout->addRow("", testRow);

    syncPoliciesGroup_ = new QGroupBox("同步策略", scrollContent);
    auto* spLayout = new QVBoxLayout(syncPoliciesGroup_);
    auto* spHint = new QLabel(
        "顶层 sync_policies：未设置的文件夹/文件使用默认策略；"
        "全部留空则保存为空（使用默认 incremental_add）。",
        syncPoliciesGroup_);
    spHint->setStyleSheet("color: gray; font-size: 11px;");
    spHint->setWordWrap(true);
    syncPoliciesEditor_ = new SyncPoliciesEditor(syncPoliciesGroup_);
    spLayout->addWidget(spHint);
    spLayout->addWidget(syncPoliciesEditor_);

    customModsGroup_ = new QGroupBox("自定义模组", scrollContent);
    auto* cmLayout = new QFormLayout(customModsGroup_);
    customModsPathEdit_ = new QLineEdit(customModsGroup_);
    customModsPathEdit_->setPlaceholderText("custom/{branch}/mods");
    cmLayout->addRow("模组路径:", customModsPathEdit_);

    mainLayout->addWidget(workspaceGroup_);
    mainLayout->addWidget(gitGroup_);
    mainLayout->addWidget(syncPoliciesGroup_);
    mainLayout->addWidget(customModsGroup_);
    mainLayout->addStretch();

    scrollArea->setWidget(scrollContent);

    saveBtn_ = new QPushButton("保存仓库配置", this);
    saveBtn_->setMinimumHeight(36);

    outerLayout->addWidget(scrollArea, 1);
    outerLayout->addWidget(saveBtn_);
}

void RepoEditor::connectSignals()
{
    connect(testBtn_, &QPushButton::clicked, this, &RepoEditor::onTestConnection);
    connect(saveBtn_, &QPushButton::clicked, [this]() {
        nlohmann::json config = saveToJson();
        emit saveRequested(QString::fromStdString(config.dump()));
        modified_ = false;
        CLogger::Info("RepoEditor: repo config saved");
    });

    connect(nameEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(minecraftEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(modloaderCombo_, &QComboBox::currentTextChanged, this, &RepoEditor::onCellChanged);
    connect(modloaderVersionEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(sshEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(httpsEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(httpEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(primaryCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &RepoEditor::onCellChanged);
    connect(defaultBranchEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(customModsPathEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
    connect(syncPoliciesEditor_, &SyncPoliciesEditor::contentModified, this, &RepoEditor::onCellChanged);
}

void RepoEditor::loadFromJson(const nlohmann::json& config)
{
    try {
        if (config.contains("workspace")) {
            auto& ws = config["workspace"];
            nameEdit_->setText(QString::fromStdString(ws.value("name", "")));
            minecraftEdit_->setText(QString::fromStdString(ws.value("minecraft", "")));
            modloaderVersionEdit_->setText(
                QString::fromStdString(ws.value("modloader_version", "")));
            QString modloader = QString::fromStdString(ws.value("modloader", "fabric"));
            int idx = modloaderCombo_->findText(modloader);
            if (idx >= 0) {
                modloaderCombo_->setCurrentIndex(idx);
            }
        }

        if (config.contains("git")) {
            auto& git = config["git"];
            sshEdit_->clear();
            httpsEdit_->clear();
            httpEdit_->clear();
            std::string remote = git.value("remote", "");
            std::string primary = "ssh";
            if (git.contains("remotes") && git["remotes"].is_array()) {
                for (const auto& entry : git["remotes"]) {
                    const std::string type = entry.value("type", "");
                    const std::string url = entry.value("url", "");
                    if (url.empty()) continue;
                    if (type == "ssh") sshEdit_->setText(QString::fromStdString(url));
                    else if (type == "https") httpsEdit_->setText(QString::fromStdString(url));
                    else if (type == "http") httpEdit_->setText(QString::fromStdString(url));
                }
            }
            if (sshEdit_->text().isEmpty() && httpsEdit_->text().isEmpty()
                && httpEdit_->text().isEmpty() && !remote.empty()) {
                if (remote.rfind("git@", 0) == 0 || remote.rfind("ssh://", 0) == 0) {
                    sshEdit_->setText(QString::fromStdString(remote));
                } else if (remote.rfind("http://", 0) == 0) {
                    httpEdit_->setText(QString::fromStdString(remote));
                } else {
                    httpsEdit_->setText(QString::fromStdString(remote));
                    primary = "https";
                }
            }
            if (remote == httpsEdit_->text().toStdString()) primary = "https";
            else if (remote == httpEdit_->text().toStdString()) primary = "http";
            else if (remote == sshEdit_->text().toStdString()) primary = "ssh";
            const int idx = primaryCombo_->findData(QString::fromStdString(primary));
            if (idx >= 0) primaryCombo_->setCurrentIndex(idx);
            defaultBranchEdit_->setText(
                QString::fromStdString(git.value("default_branch", "main")));
        }

        if (config.contains("sync_policies")) {
            syncPoliciesEditor_->load(config["sync_policies"]);
        } else {
            syncPoliciesEditor_->load(nullptr);
        }

        if (config.contains("custom_mods")) {
            auto& cm = config["custom_mods"];
            customModsPathEdit_->setText(
                QString::fromStdString(cm.value("path", "")));
        }
    } catch (const std::exception& e) {
        CLogger::Error("RepoEditor::loadFromJson failed: {}", e.what());
    }

    modified_ = false;
}

QString RepoEditor::primaryRepoUrl() const
{
    const QString type = primaryCombo_->currentData().toString();
    if (type == "ssh") return sshEdit_->text().trimmed();
    if (type == "http") return httpEdit_->text().trimmed();
    if (type == "https") return httpsEdit_->text().trimmed();
    return {};
}

nlohmann::json RepoEditor::saveToJson() const
{
    nlohmann::json config;

    config["workspace"]["name"] = nameEdit_->text().toStdString();
    config["workspace"]["minecraft"] = minecraftEdit_->text().toStdString();
    config["workspace"]["modloader"] = modloaderCombo_->currentText().toStdString();
    config["workspace"]["modloader_version"] = modloaderVersionEdit_->text().toStdString();

    config["version"] = "1.1";

    std::vector<nlohmann::json> remotes;
    const auto addRemote = [&remotes](const std::string& type, const QString& value) {
        if (value.trimmed().isEmpty()) return;
        remotes.push_back({{"type", type}, {"url", value.trimmed().toStdString()}});
    };
    const QString primary = primaryCombo_->currentData().toString();
    const auto addByType = [&](const std::string& type) {
        if (type == "ssh") addRemote("ssh", sshEdit_->text());
        else if (type == "https") addRemote("https", httpsEdit_->text());
        else if (type == "http") addRemote("http", httpEdit_->text());
    };
    addByType(primary.toStdString());
    for (const auto& type : { "ssh", "https", "http" }) {
        if (primary != QString::fromLatin1(type)) addByType(type);
    }
    config["git"]["remotes"] = nlohmann::json(remotes);
    config["git"]["remote"] = remotes.empty() ? "" : remotes.front().value("url", "");
    config["git"]["default_branch"] = defaultBranchEdit_->text().toStdString();

    config["custom_mods"]["enabled"] = !customModsPathEdit_->text().isEmpty();
    config["custom_mods"]["path"] = customModsPathEdit_->text().toStdString();

    nlohmann::json sp = syncPoliciesEditor_->save();
    config["sync_policies"] = sp.is_null() ? nlohmann::json(nullptr) : std::move(sp);

    return config;
}

bool RepoEditor::isModified() const
{
    return modified_;
}

void RepoEditor::onTestConnection()
{
    testBtn_->setEnabled(false);
    testResult_->setText("正在测试...");
    testResult_->setStyleSheet("color: gray;");
    emit connectionTestClicked(primaryRepoUrl());
}

void RepoEditor::onCellChanged()
{
    markModified();
}

void RepoEditor::markModified()
{
    if (!modified_) {
        modified_ = true;
        emit contentModified();
    }
}

} // namespace GUIWorker

#include "repo_editor.moc"


