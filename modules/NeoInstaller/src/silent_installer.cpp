#include "silent_installer.h"
#include "git_downloader.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QProcess>
#include <QObject>
#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <objbase.h>
#endif

namespace NeoInstaller {

static bool systemGitAvailable()
{
    QProcess p;
    p.start("git", {"--version"});
    p.waitForFinished(5000);
    return p.exitCode() == 0;
}

static bool downloadAndInstallGit(const std::string& installPath, bool fullGit)
{
    QString pattern = fullGit ? QStringLiteral("PortableGit") : QStringLiteral("MinGit");
    QString psCmd = QString(
        "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;"
        "$r=Invoke-RestMethod -Uri 'https://api.github.com/repos/git-for-windows/git/releases/latest';"
        "foreach($a in $r.assets){if($a.name -like '*%1*64-bit*'){"
        "Write-Output $a.browser_download_url;break}}"
    ).arg(pattern);

    QProcess ps;
    ps.start("powershell", {"-NoProfile", "-Command", psCmd});
    ps.waitForFinished(30000);
    QByteArray out = ps.readAllStandardOutput().trimmed();
    std::cout << "[GIT] PS exit=" << ps.exitCode() << " out(" << out.size() << ")" << std::endl;

    QString url = QString::fromUtf8(out);
    if (url.isEmpty() || !url.startsWith("http")) {
        std::cerr << "[ERROR] No valid URL from GitHub API" << std::endl;
        QByteArray err = ps.readAllStandardError().trimmed();
        if (!err.isEmpty()) std::cerr << "[GIT] PS stderr: " << err.toStdString() << std::endl;
        return false;
    }
    std::cout << "[GIT] " << url.toStdString() << std::endl;

    // Download: .zip or .7z.exe
    bool is7zExe = url.endsWith(".7z.exe", Qt::CaseInsensitive);
    QString dlPath = QDir::tempPath() + (is7zExe ? "/neo_git.7z.exe" : "/neo_git.zip");
    {
        QProcess dl;
        dl.start("powershell", {"-Command", "Invoke-WebRequest", "-Uri", url, "-OutFile", dlPath});
        dl.waitForFinished(300000);
        if (dl.exitCode() != 0 || !QFile::exists(dlPath)) {
            std::cerr << "[ERROR] Download failed" << std::endl;
            return false;
        }
    }

    std::cout << "[GIT] Extracting..." << std::endl;
    QString gitDir = QString::fromStdString(installPath) + "/tools/git";
    QDir().mkpath(gitDir);

    if (is7zExe) {
        // 提取内置 7za.exe 到临时目录
        QString sevenZipPath = QDir::tempPath() + "/neo_7za.exe";
        {
            QFile src(":/tools/7za.exe");
            QFile dst(sevenZipPath);
            if (src.open(QIODevice::ReadOnly) && dst.open(QIODevice::WriteOnly)) {
                dst.write(src.readAll());
                dst.close();
            }
        }
        // 去 .exe 后缀 → .7z
        QString archivePath = dlPath.chopped(4);
        QFile::rename(dlPath, archivePath);
        // 使用 7za.exe x archive -o<dir>
        QProcess sevenZip;
        sevenZip.start(sevenZipPath, {"x", archivePath, "-o" + gitDir, "-y"});
        sevenZip.waitForFinished(120000);
        QFile::remove(archivePath);
        QFile::remove(sevenZipPath);
    } else {
        QProcess tar;
        tar.setWorkingDirectory(gitDir);
        tar.start("tar", {"-xf", dlPath});
        tar.waitForFinished(120000);
        if (tar.exitCode() != 0) {
            QProcess ps2;
            ps2.start("powershell", {"-Command", "Expand-Archive", "-Path", dlPath,
                "-DestinationPath", gitDir, "-Force"});
            ps2.waitForFinished(120000);
        }
        QFile::remove(dlPath);
    }
    std::cout << "[GIT] Installed to " << gitDir.toStdString() << std::endl;
    return true;
}

static bool extractDeployFiles(const QString& targetDir)
{
    QDirIterator it(":/deploy/", QDirIterator::Subdirectories);
    int count = 0;

    while (it.hasNext()) {
        QString path = it.next();
        QFileInfo fi(path);
        if (fi.isDir()) continue;

        QString relativePath = path.mid(QStringLiteral(":/deploy/").length());
        if (relativePath.isEmpty()) continue;

        QString destPath = QDir(targetDir).absoluteFilePath(relativePath);
        QDir().mkpath(QFileInfo(destPath).absolutePath());

        QFile src(path);
        QFile dst(destPath);
        if (!src.open(QIODevice::ReadOnly)) { continue; }
        if (!dst.open(QIODevice::WriteOnly)) { continue; }

        dst.write(src.readAll());
        dst.close();
        src.close();
        ++count;
    }

    std::cout << "[OK] Extracted " << count << " files" << std::endl;
    return count > 0;
}

SilentInstaller::SilentInstaller() {}

bool SilentInstaller::run(const std::string& installPath)
{
    if (installPath.empty())
        installPath_ = getDefaultInstallPath();
    else
        installPath_ = installPath;

    std::cout << "[INSTALL] Target: " << installPath_ << std::endl;

    if (!createDirectories(installPath_)) {
        std::cerr << "[ERROR] Failed to create install directory" << std::endl;
        return false;
    }

    std::cout << "[INSTALL] Extracting files..." << std::endl;
    if (!extractDeployFiles(QString::fromStdString(installPath_))) {
        std::cerr << "[ERROR] Failed to extract files" << std::endl;
        return false;
    }

    bool needInstall = true;
    if (gitMode_ == GitMode::UseSystem) {
        if (systemGitAvailable())
            needInstall = false;
        else
            std::cerr << "[WARN] --use-system-git but no system Git found" << std::endl;
    } else if (gitMode_ == GitMode::Auto) {
        if (systemGitAvailable()) {
            std::cout << "[GIT] Using system Git" << std::endl;
            needInstall = false;
        }
    }
    // GitMode::UseBundled: always install

    if (needInstall) {
        std::cout << "[GIT] Installing bundled Git..." << std::endl;
        if (!downloadAndInstallGit(installPath_, installEditor_))
            std::cerr << "[WARN] Git installation failed" << std::endl;
    }

    createShortcuts(installPath_);
    writeInstallConfig(installPath_);

    std::cout << "[OK] Installation complete: " << installPath_ << std::endl;
    return true;
}

bool SilentInstaller::createDirectories(const std::string& path)
{
    return QDir(QString::fromStdString(path)).mkpath(".");
}

std::string SilentInstaller::getDefaultInstallPath()
{
#ifdef _WIN32
    char pf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, pf)))
        return std::string(pf) + "\\NeoServer";
#endif
    return "C:\\NeoServer";
}

bool SilentInstaller::createShortcuts(const std::string& ip)
{
    std::string exe = ip + "\\NeoServerUpdateModpack.exe";
    createDesktopShortcut(exe, ip);
    createStartMenuShortcut(exe, ip);
    if (installEditor_)
        createDesktopShortcut(ip + "\\NeoWorkspaceEditor.exe", ip);
    return true;
}

void SilentInstaller::writeInstallConfig(const std::string& ip)
{
    std::ofstream c(ip + "/install.conf");
    c << "# NeoServer Install Configuration\n";
    c << "install_path=" << ip << "\n";
    c << "install_editor=" << (installEditor_ ? "true" : "false") << "\n";
    c << "use_system_git=" << (systemGitAvailable() ? "true" : "false") << "\n";
    c << "git_path=" << ip << "\\tools\\git\\bin\\git.exe\n";
    c.close();
    std::cout << "[OK] install.conf written" << std::endl;
}

bool SilentInstaller::createDesktopShortcut(const std::string& target, const std::string& workDir)
{
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IShellLinkW* sl = nullptr; IPersistFile* pf = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl);
    if (SUCCEEDED(hr)) {
        QString qtarget = QString::fromStdString(target);
        QString qdir = QString::fromStdString(workDir);
        sl->SetPath(qtarget.toStdWString().c_str());
        sl->SetWorkingDirectory(qdir.toStdWString().c_str());
        hr = sl->QueryInterface(IID_IPersistFile, (void**)&pf);
        if (SUCCEEDED(hr)) {
            WCHAR dp[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, dp))) {
                std::wstring sp = std::wstring(dp) + L"\\NeoServer.lnk";
                pf->Save(sp.c_str(), TRUE);
            }
            pf->Release();
        }
        sl->Release();
    }
    return SUCCEEDED(hr);
#else
    return false;
#endif
}

bool SilentInstaller::createStartMenuShortcut(const std::string& target, const std::string& workDir)
{
#ifdef _WIN32
    WCHAR pp[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, pp))) return false;
    std::wstring dp = std::wstring(pp) + L"\\NeoServer";
    CreateDirectoryW(dp.c_str(), nullptr);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
    IShellLinkW* sl = nullptr; IPersistFile* pf = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl);
    if (SUCCEEDED(hr)) {
        QString qtarget = QString::fromStdString(target);
        QString qdir = QString::fromStdString(workDir);
        sl->SetPath(qtarget.toStdWString().c_str());
        sl->SetWorkingDirectory(qdir.toStdWString().c_str());
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

} // namespace NeoInstaller
