#include "repo_editor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QHeaderView>
#include <QSplitter>

#include <logger.h>

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
    remoteEdit_ = new QLineEdit(gitGroup_);
    remoteEdit_->setPlaceholderText("https://github.com/user/repo.git");
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

    gitLayout->addRow("远程地址:", remoteEdit_);
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
    connect(remoteEdit_, &QLineEdit::textChanged, this, &RepoEditor::onCellChanged);
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
            remoteEdit_->setText(QString::fromStdString(git.value("remote", "")));
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

nlohmann::json RepoEditor::saveToJson() const
{
    nlohmann::json config;

    config["workspace"]["name"] = nameEdit_->text().toStdString();
    config["workspace"]["minecraft"] = minecraftEdit_->text().toStdString();
    config["workspace"]["modloader"] = modloaderCombo_->currentText().toStdString();
    config["workspace"]["modloader_version"] = modloaderVersionEdit_->text().toStdString();

    config["git"]["remote"] = remoteEdit_->text().toStdString();
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
    emit connectionTestClicked(remoteEdit_->text());
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


