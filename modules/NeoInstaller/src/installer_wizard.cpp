#include "installer_wizard.h"
#include "git_downloader.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>
#include <QTimer>
#include <QSettings>
#include <QStyle>
#include <QGroupBox>
#include <fstream>
#include <QProcess>
#include <QRadioButton>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#endif

namespace NeoInstaller {

// ============================================================
// WelcomePage
// ============================================================
WelcomePage::WelcomePage(QWidget* parent) : QWizardPage(parent) {
    setTitle("欢迎使用 NeoServer 安装程序");
    setSubTitle("本向导将引导您完成安装。");
    auto* l = new QVBoxLayout(this);
    auto* info = new QLabel(
        "NeoServer 整合包管理工具 v1.0\n\n"
        "  • NeoServerUpdateModpack — 核心程序\n"
        "  • NeoWorkspaceEditor — 工作区编辑器（可选）\n"
        "  • Git 版本控制", this);
    info->setWordWrap(true);
    l->addWidget(info);
    setLayout(l);
}

// ============================================================
// LicensePage
// ============================================================
LicensePage::LicensePage(QWidget* parent) : QWizardPage(parent) {
    setTitle("许可协议");
    setSubTitle("请阅读并接受以下许可协议 (GPL v3)。");
    auto* l = new QVBoxLayout(this);
    auto* text = new QTextEdit(this);
    text->setReadOnly(true);
    QFile lic(":/license/LICENSE");
    if (lic.open(QIODevice::ReadOnly | QIODevice::Text))
        text->setPlainText(QString::fromUtf8(lic.readAll()));
    else
        text->setPlainText("GNU General Public License v3 — 许可文件未能加载。");
    acceptCheck_ = new QCheckBox("我接受许可协议", this);
    connect(acceptCheck_, &QCheckBox::toggled, this, &QWizardPage::completeChanged);
    l->addWidget(text);
    l->addWidget(acceptCheck_);
    setLayout(l);
}
bool LicensePage::isComplete() const { return acceptCheck_->isChecked(); }

// ============================================================
// InstallPathPage
// ============================================================
InstallPathPage::InstallPathPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("选择安装位置");
    setSubTitle("请选择 NeoServer 的安装目录。");
    auto* l = new QVBoxLayout(this);
    pathEdit_ = new QLineEdit("C:\\Program Files\\NeoServer", this);
    browseBtn_ = new QPushButton("浏览...", this);
    connect(browseBtn_, &QPushButton::clicked, this, &InstallPathPage::onBrowse);
    auto* hl = new QHBoxLayout();
    hl->addWidget(pathEdit_);
    hl->addWidget(browseBtn_);
    l->addLayout(hl);
    setLayout(l);
    registerField("installPath*", pathEdit_);
}
bool InstallPathPage::isComplete() const { return !pathEdit_->text().isEmpty(); }
bool InstallPathPage::validatePage() {
    QDir d(pathEdit_->text());
    if (d.exists()) {
        auto ents = d.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!ents.isEmpty()) {
            int r = QMessageBox::warning(this, "目标目录非空",
                QString("目标目录已存在且包含 %1 个文件/文件夹。\n\n"
                    "安装过程将清空该目录的全部内容。\n\n确定继续吗？").arg(ents.size()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            return r == QMessageBox::Yes;
        }
    }
    return true;
}
void InstallPathPage::onBrowse() {
    QString d = QFileDialog::getExistingDirectory(this, "选择安装目录", pathEdit_->text());
    if (!d.isEmpty()) pathEdit_->setText(d);
}

// ============================================================
// ComponentPage (Git 必装, 仅编辑器可选)
// ============================================================
ComponentPage::ComponentPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("选择组件");
    setSubTitle("Git 为必装组件，编辑器为可选。");
    auto* l = new QVBoxLayout(this);
    auto* gb = new QGroupBox("组件", this);
    auto* gl = new QVBoxLayout(gb);
    auto* core = new QCheckBox("核心程序 — 必装", gb);
    core->setChecked(true); core->setEnabled(false);
    gl->addWidget(core);
    editorCheck_ = new QCheckBox("工作区编辑器", gb);
    gl->addWidget(editorCheck_);
    auto* gi = new QLabel("Git 版本控制 — 必装", gb);
    gi->setStyleSheet("color: #666;");
    gl->addWidget(gi);
    l->addWidget(gb);
    setLayout(l);
}
void ComponentPage::initializePage() {
    auto* w = qobject_cast<InstallerWizard*>(wizard());
    if (w) editorCheck_->setChecked(w->installEditor());
}
int ComponentPage::nextId() const { return InstallerWizard::PAGE_GIT_CHOICE; }
bool ComponentPage::isEditorSelected() const { return editorCheck_->isChecked(); }

// ============================================================
// GitChoicePage
// ============================================================
GitChoicePage::GitChoicePage(QWidget* parent) : QWizardPage(parent), systemGitAvailable_(false) {
    setTitle("Git 版本控制");
    setSubTitle("选择 Git 使用方式。");
    auto* l = new QVBoxLayout(this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    l->addWidget(statusLabel_);
    systemGitRadio_ = new QRadioButton("使用系统 Git", this);
    bundledGitRadio_ = new QRadioButton("安装内置 Git（将下载最新版）", this);
    bundledGitRadio_->setChecked(true);
    l->addWidget(systemGitRadio_);
    l->addWidget(bundledGitRadio_);
    setLayout(l);
}
void GitChoicePage::initializePage() {
    systemGitAvailable_ = GitDownloader::isSystemGitAvailable();
    if (systemGitAvailable_) {
        statusLabel_->setText("检测到系统 Git:\n  " + GitDownloader::systemGitPath()
            + "\n  版本: " + GitDownloader::systemGitVersion());
        systemGitRadio_->setEnabled(true);
        systemGitRadio_->setChecked(true);
    } else {
        statusLabel_->setText("未检测到系统 Git。将安装内置 Git。");
        systemGitRadio_->setEnabled(false);
        bundledGitRadio_->setChecked(true);
    }
}
int GitChoicePage::nextId() const {
    return bundledGitRadio_->isChecked() ? InstallerWizard::PAGE_GIT_LICENSE
        : InstallerWizard::PAGE_PROGRESS;
}
bool GitChoicePage::useSystemGit() const { return systemGitRadio_->isChecked(); }

// ============================================================
// GitLicensePage
// ============================================================
GitLicensePage::GitLicensePage(QWidget* parent) : QWizardPage(parent) {
    setTitle("Git 许可协议");
    setSubTitle("安装 Git 需要同意 GNU General Public License v2。");
    auto* l = new QVBoxLayout(this);
    auto* text = new QTextEdit(this);
    text->setReadOnly(true);
    text->setPlainText(
        "GNU GENERAL PUBLIC LICENSE\n"
        "Version 2, June 1991\n\n"
        "Copyright (C) 1989, 1991 Free Software Foundation, Inc.\n"
        "51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA\n\n"
        "Everyone is permitted to copy and distribute verbatim copies\n"
        "of this license document, but changing it is not allowed.\n\n"
        "Preamble\n\n"
        "The licenses for most software are designed to take away your\n"
        "freedom to share and change it. By contrast, the GNU General Public\n"
        "License is intended to guarantee your freedom to share and change\n"
        "free software--to make sure the software is free for all its users.\n\n"
        "TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\n\n"
        "0. This License applies to any program or other work which contains\n"
        "a notice placed by the copyright holder saying it may be distributed\n"
        "under the terms of this General Public License.\n\n"
        "1. You may copy and distribute verbatim copies of the Program's\n"
        "source code as you receive it, in any medium.\n\n"
        "2. You may modify your copy or copies of the Program or any portion\n"
        "of it, thus forming a work based on the Program.\n\n"
        "3. You may copy and distribute the Program (or a work based on it)\n"
        "in object code or executable form under the terms of Sections 1\n"
        "and 2 above.\n\n"
        "NO WARRANTY\n\n"
        "11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO\n"
        "WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.\n\n"
        "Full text: https://www.gnu.org/licenses/gpl-2.0.html");
    acceptCheck_ = new QCheckBox("我同意 Git 使用协议 (GPL v2)", this);
    connect(acceptCheck_, &QCheckBox::toggled, this, &QWizardPage::completeChanged);
    l->addWidget(text);
    l->addWidget(acceptCheck_);
    setLayout(l);
}
bool GitLicensePage::isComplete() const { return acceptCheck_->isChecked(); }

// ============================================================
// ProgressPage
// ============================================================
ProgressPage::ProgressPage(QWidget* parent) : QWizardPage(parent),
    installComplete_(false), installSuccess_(false), dlProc_(nullptr)
{
    setTitle("正在安装");
    setSubTitle("请等待安装完成...");
    auto* l = new QVBoxLayout(this);
    statusIcon_ = new QLabel(this);
    statusIcon_->setAlignment(Qt::AlignCenter);
    statusIcon_->setVisible(false);
    progressBar_ = new QProgressBar(this);
    progressBar_->setRange(0, 100);
    stepLabel_ = new QLabel("准备中...", this);
    logView_ = new QTextEdit(this);
    logView_->setReadOnly(true);
    logView_->setMaximumHeight(180);
    l->addWidget(statusIcon_);
    l->addWidget(progressBar_);
    l->addWidget(stepLabel_);
    l->addWidget(logView_);
    setLayout(l);
}
void ProgressPage::initializePage() {
    installComplete_ = false;
    installSuccess_ = false;
    logView_->clear();
    QTimer::singleShot(0, this, [this]() {
        if (auto* btn = wizard()->button(QWizard::BackButton))
            btn->hide();
        if (auto* btn = wizard()->button(QWizard::CancelButton))
            btn->setEnabled(false);
    });
    QTimer::singleShot(100, this, &ProgressPage::performInstallation);
}
bool ProgressPage::isComplete() const { return installComplete_; }
QString ProgressPage::fieldInstallPath() const { return field("installPath").toString(); }
void ProgressPage::updateProgress(int pct, const QString& step, const QString& detail) {
    progressBar_->setValue(pct);
    stepLabel_->setText(step);
    if (!detail.isEmpty()) logView_->append(detail);
    QApplication::processEvents();
}

void ProgressPage::performInstallation() {
    QString installPath = fieldInstallPath();
    auto* wiz = qobject_cast<InstallerWizard*>(wizard());
    QDir dir(installPath);
    bool failed = false;

    // 1. 准备目录 + 清空
    updateProgress(3, "准备安装目录...", installPath);
    if (dir.exists()) {
        auto ents = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        if (!ents.isEmpty()) {
            logView_->append(QString("清空 %1 个已有条目...").arg(ents.size()));
            if (!dir.removeRecursively()) {
                logView_->append("错误: 无法清空目录！"); failed = true;
            }
        }
    }
    if (!failed && !dir.mkpath(".")) {
        logView_->append("错误: 无法创建目录！"); failed = true;
    }

    // 2. 释放文件
    if (!failed) {
        updateProgress(5, "释放文件...", "");
        int total = 0, done = 0;
        { QDirIterator it(":/deploy/", QDirIterator::Subdirectories);
          while (it.hasNext()) { it.next(); ++total; } }
        QDirIterator fit(":/deploy/", QDirIterator::Subdirectories);
        while (!failed && fit.hasNext()) {
            QString path = fit.next();
            QFileInfo fi(path);
            if (fi.isDir()) continue;
            QString rel = path.mid(QStringLiteral(":/deploy/").length());
            if (rel.isEmpty()) continue;
            QString dest = dir.absoluteFilePath(rel);
            QDir().mkpath(QFileInfo(dest).absolutePath());
            QFile src(path), dst(dest);
            if (src.open(QIODevice::ReadOnly) && dst.open(QIODevice::WriteOnly)) {
                dst.write(src.readAll());
                dst.close(); src.close(); ++done;
            }
            if (total > 0) {
                updateProgress(5 + done * 55 / total, "释放文件...",
                    QString("[%1/%2] %3").arg(done).arg(total).arg(rel));
            }
        }
        logView_->append(QString("已释放 %1 个文件").arg(done));
    }

    // 3. Git
    bool needGit = true;
    if (!failed && wiz) {
        auto* g = qobject_cast<GitChoicePage*>(wiz->page(InstallerWizard::PAGE_GIT_CHOICE));
        if (g) needGit = !g->useSystemGit();
    }

    if (!failed && needGit) {
        auto variant = (wiz && wiz->installEditor()) ? GitVariant::PortableGit : GitVariant::MinGit;
        QString url = GitDownloader::fetchLatestGitUrl(variant);
        logView_->append("Git URL: " + (url.isEmpty() ? QString("获取失败") : url));
        if (url.isEmpty()) {
            logView_->append("警告: 无法获取 Git 下载地址");
        } else {
            QString zip = QDir::tempPath() + "/neo_git.zip";
            logView_->append("下载 Git: " + url.left(70) + "...");

            dlProc_ = new QProcess(this);
            connect(dlProc_, &QProcess::readyReadStandardOutput, this, [this]() {
                logView_->append(QString::fromLocal8Bit(dlProc_->readAllStandardOutput()).trimmed());
            });
            connect(dlProc_, &QProcess::readyReadStandardError, this, [this]() {
                logView_->append(QString::fromLocal8Bit(dlProc_->readAllStandardError()).trimmed());
            });

            QEventLoop loop;
            connect(dlProc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                &loop, &QEventLoop::quit);
            dlProc_->start("powershell", {"-Command",
                "Invoke-WebRequest", "-Uri", url, "-OutFile", zip});
            loop.exec();

            if (dlProc_->exitCode() == 0 && QFile::exists(zip)) {
                updateProgress(72, "安装 Git...", "");
                QString gitDir = installPath + "/tools/git";
                QDir().mkpath(gitDir);

                QProcess tar;
                tar.setWorkingDirectory(gitDir);
                tar.start("tar", {"-xf", zip});
                tar.waitForFinished(120000);
                if (tar.exitCode() != 0) {
                    QProcess ps;
                    ps.start("powershell", {"-Command",
                        "Expand-Archive", "-Path", zip, "-DestinationPath", gitDir, "-Force"});
                    ps.waitForFinished(120000);
                }
                logView_->append("Git 已安装: " + gitDir);
                QFile::remove(zip);
            } else {
                logView_->append("Git 下载失败");
            }
            dlProc_->deleteLater();
            dlProc_ = nullptr;
        }
    } else if (!failed) {
        logView_->append("使用系统 Git: " + GitDownloader::systemGitPath());
    }

    // 4. 快捷方式 + 配置
    if (!failed) {
        updateProgress(80, "创建快捷方式...", "");
        createDesktopShortcut(installPath + "/NeoServerUpdateModpack.exe", installPath);
        createStartMenuShortcut(installPath + "/NeoServerUpdateModpack.exe", installPath);
        updateProgress(90, "写入安装配置...", "install.conf");
        writeInstallConfig(installPath);
    }

    updateProgress(failed ? 0 : 100, failed ? "安装失败" : "安装完成！", "");
    statusIcon_->setText(failed ? "安装失败" : "安装成功");
    statusIcon_->setStyleSheet(failed
        ? "color: red; font-size: 24px; font-weight: bold;"
        : "color: green; font-size: 24px; font-weight: bold;");
    statusIcon_->setVisible(true);
    installSuccess_ = !failed;
    installComplete_ = true;
    emit completeChanged();
}

bool ProgressPage::createDesktopShortcut(const QString& target, const QString& workDir) {
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IShellLink* sl = nullptr; IPersistFile* pf = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&sl);
    if (SUCCEEDED(hr)) {
        sl->SetPath(target.toStdWString().c_str());
        sl->SetWorkingDirectory(workDir.toStdWString().c_str());
        hr = sl->QueryInterface(IID_IPersistFile, (void**)&pf);
        if (SUCCEEDED(hr)) {
            WCHAR dp[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, dp)))
                pf->Save((std::wstring(dp) + L"\\NeoServer.lnk").c_str(), TRUE);
            pf->Release();
        }
        sl->Release();
    }
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

bool ProgressPage::createStartMenuShortcut(const QString& target, const QString& workDir) {
#ifdef _WIN32
    WCHAR pp[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, pp))) return false;
    std::wstring dp = std::wstring(pp) + L"\\NeoServer";
    CreateDirectoryW(dp.c_str(), nullptr);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IShellLink* sl = nullptr; IPersistFile* pf = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&sl);
    if (SUCCEEDED(hr)) {
        sl->SetPath(target.toStdWString().c_str());
        sl->SetWorkingDirectory(workDir.toStdWString().c_str());
        hr = sl->QueryInterface(IID_IPersistFile, (void**)&pf);
        if (SUCCEEDED(hr)) {
            pf->Save((dp + L"\\NeoServer.lnk").c_str(), TRUE);
            pf->Release();
        }
        sl->Release();
    }
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

void ProgressPage::writeInstallConfig(const QString& installPath) {
    std::string p = installPath.toStdString();
    std::ofstream c(p + "/install.conf");
    c << "# NeoServer Install Configuration\n";
    c << "install_path=" << p << "\n";
    auto* wiz = qobject_cast<InstallerWizard*>(wizard());
    if (wiz) {
        auto* cp = qobject_cast<ComponentPage*>(wiz->page(InstallerWizard::PAGE_COMPONENTS));
        c << "install_editor=" << (cp && cp->isEditorSelected() ? "true" : "false") << "\n";
        auto* gp = qobject_cast<GitChoicePage*>(wiz->page(InstallerWizard::PAGE_GIT_CHOICE));
        c << "use_system_git=" << (gp && gp->useSystemGit() ? "true" : "false") << "\n";
        c << "git_path=" << p << "\\tools\\git\\bin\\git.exe\n";
    }
    c.close();
    logView_->append("install.conf 已写入");
}

// ============================================================
// FinishPage
// ============================================================
FinishPage::FinishPage(QWidget* parent) : QWizardPage(parent) {
    setTitle("安装完成");
    auto* l = new QVBoxLayout(this);
    finishLabel_ = new QLabel("安装已完成！", this);
    finishLabel_->setStyleSheet("font-size: 16px; font-weight: bold; color: green;");
    launchCheck_ = new QCheckBox("立即运行 NeoServer", this);
    launchCheck_->setChecked(true);
    l->addWidget(finishLabel_);
    l->addWidget(launchCheck_);
    setLayout(l);
}
void FinishPage::initializePage() {
    auto* pp = qobject_cast<ProgressPage*>(wizard()->page(InstallerWizard::PAGE_PROGRESS));
    if (pp && !pp->installSuccess_) {
        finishLabel_->setText("安装过程中出现错误。");
        finishLabel_->setStyleSheet("font-size: 16px; font-weight: bold; color: red;");
    }
}

// ============================================================
// InstallerWizard
// ============================================================
InstallerWizard::InstallerWizard(QWidget* parent) : QWizard(parent) {
    setWindowTitle("NeoServer 安装程序");
    setWizardStyle(QWizard::ModernStyle);
    setPage(PAGE_WELCOME, new WelcomePage(this));
    setPage(PAGE_LICENSE, new LicensePage(this));
    setPage(PAGE_INSTALL_PATH, new InstallPathPage(this));
    setPage(PAGE_COMPONENTS, new ComponentPage(this));
    setPage(PAGE_GIT_CHOICE, new GitChoicePage(this));
    setPage(PAGE_GIT_LICENSE, new GitLicensePage(this));
    setPage(PAGE_PROGRESS, new ProgressPage(this));
    setPage(PAGE_FINISH, new FinishPage(this));
    setStartId(PAGE_WELCOME);
    setButtonText(QWizard::NextButton, "下一步");
    setButtonText(QWizard::BackButton, "上一步");
    setButtonText(QWizard::CancelButton, "取消");
    setButtonText(QWizard::FinishButton, "完成");
}
InstallerWizard::~InstallerWizard() {}

} // namespace NeoInstaller

#include "installer_wizard.moc"
