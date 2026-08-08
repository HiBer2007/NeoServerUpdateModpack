#include "cli_dispatcher.h"
#include "cli_output.h"

#include <logger.h>
#include <error_codes.h>
#include <PluginLoader.h>
#include <IConfigParser.h>
#include <IModpackExporter.h>

#include <workspace_manager.h>
#include <git_operations.h>
#include <history_store.h>

#include <build_engine.h>
#include <modpack_exporter.h>
#include <serverconfig_sync.h>
#include <pointer_downloader.h>
#include <IPluginPointer.h>
#include <platform_api.h>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <regex>
#include <cctype>
#include <cstdlib>
#include <csignal>
#include <thread>
#include <chrono>

#include <QProcess>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QString>
#include <QSysInfo>
#include <QUrl>
#include <QCoreApplication>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace NeoCLI {

static CliDispatcher* g_activeDispatcher = nullptr;

static void signalHandler(int signum)
{
    if (g_activeDispatcher) {
        g_activeDispatcher->cancel();
    }
    if (signum == SIGINT) {
        std::signal(SIGINT, signalHandler);
    }
}

static std::string slugifyRepo(const std::string& url)
{
    std::string s = url;
    for (auto& c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-' && c != '_') {
            c = '_';
        }
    }
    if (s.size() > 64) {
        s = s.substr(0, 64);
    }
    return s;
}

// IBuildProgress 的 CLI 适配器：进度/日志输出到终端，取消走 CancelToken。
class CliBuildProgress : public NeoCore::IBuildProgress {
public:
    explicit CliBuildProgress(NeoCore::CancelToken* token)
        : token_(token)
    {
    }

    void set_main_stage(const std::string& stage) override { stage_ = stage; }
    void set_main_progress(int percent) override
    {
        CliOutput::progress(percent, stage_ + ": " + message_);
    }
    void set_main_message(const std::string& message) override { message_ = message; }

    int add_sub_bar(const std::string& label) override
    {
        (void)label;
        return ++nextHandle_;
    }
    void remove_sub_bar(int handle) override { (void)handle; }
    void set_sub_progress(int handle, int percent) override
    {
        (void)handle;
        (void)percent;
    }
    void set_sub_info(int handle, const std::string& info) override
    {
        (void)handle;
        (void)info;
    }

    void log(const std::string& line) override { CliOutput::info(line); }
    bool is_cancelled() const override { return token_ && token_->is_cancelled(); }

private:
    NeoCore::CancelToken* token_;
    std::string stage_;
    std::string message_;
    int nextHandle_ = 0;
};

static std::string exeDir()
{
    char buf[4096] = {};
#ifdef _WIN32
    GetModuleFileNameA(nullptr, buf, sizeof(buf));
    return fs::path(buf).parent_path().string();
#else
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf));
    if (n > 0) return fs::path(std::string(buf, n)).parent_path().string();
    return std::string(".");
#endif
}

static QString installedGitVersion(const QString& gitExe)
{
    if (gitExe.isEmpty() || !QFileInfo::exists(gitExe)) return {};
    QProcess p;
    p.start(gitExe, {"--version"});
    if (!p.waitForFinished(8000)) return {};
    if (p.exitCode() != 0) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

// MinGit ships mingw64/bin/git.exe and cmd/git.exe (no top-level bin/);
// PortableGit ships bin/git.exe. Return the first runnable one.
static QString findRunnableGit(const QString& gitDir)
{
    const QStringList candidates = {
        QDir(gitDir).absoluteFilePath("bin/git.exe"),
        QDir(gitDir).absoluteFilePath("mingw64/bin/git.exe"),
        QDir(gitDir).absoluteFilePath("cmd/git.exe"),
    };
    for (const auto& c : candidates) {
        if (!installedGitVersion(c).isEmpty()) return c;
    }
    return {};
}

// Mirrors NeoInstaller::ProgressPage::writeInstallConfig format.
static bool writeGitInstallConfigFile(const std::string& confPath,
    const std::string& installRoot, const std::string& gitExe)
{
    std::ofstream c(confPath);
    if (!c) return false;
    c << "# NeoServer Install Configuration\n";
    c << "install_path=" << installRoot << "\n";
    c << "install_editor=false\n";
    c << "use_system_git=false\n";
    c << "git_path=" << gitExe << "\n";
    c.close();
    return true;
}

// Layout detection mirrors the Installer: the installed layout puts the exe
// directly at the install root (exeDir == installPath, install.conf beside it),
// while the dev/deploy layout puts the exe in build/deploy with the root at
// exeDir/.. (build). tools/git or an existing install.conf beside the exe
// marks the installed layout.
struct GitInstallLayout {
    QString installRoot;
    std::vector<QString> confPaths;
};

static GitInstallLayout detectGitInstallLayout(const QString& exeDir)
{
    GitInstallLayout l;
    QString dir = QDir::cleanPath(exeDir);
    bool installedLayout =
        QFileInfo::exists(dir + "/tools/git") ||
        QFileInfo::exists(dir + "/install.conf");
    if (installedLayout) {
        l.installRoot = dir;
        l.confPaths = { dir + "/install.conf" };
    } else {
        l.installRoot = QDir::cleanPath(QDir(dir).absoluteFilePath(".."));
        l.confPaths = { l.installRoot + "/install.conf" };
    }
    return l;
}

// Writes install.conf to the layout's true install root only (one file,
// matching the Installer). Returns the paths actually written.
static std::vector<std::string> writeGitInstallConfFiles(
    const std::vector<QString>& confPaths,
    const std::string& installRoot, const std::string& gitExe)
{
    std::vector<std::string> out;
    for (const auto& p : confPaths) {
        std::string s = QDir::toNativeSeparators(p).toStdString();
        if (writeGitInstallConfigFile(s, installRoot, gitExe)) out.push_back(s);
    }
    return out;
}

static void fillExportMetadata(NeoCore::ExportMetadata& meta,
    const std::string& wsJson)
{
    if (wsJson.empty()) return;
    NeoWorkspace::WorkspaceManager wm;
    if (!wm.loadFromFile(wsJson)) return;
    meta.name = wm.workspaceName();
    meta.game_version = wm.minecraftVersion();
    meta.modloader = wm.modloader();
}

CliDispatcher::CliDispatcher()
{
    auto prev = std::signal(SIGINT, signalHandler);
    if (prev == SIG_IGN) {
        std::signal(SIGINT, SIG_IGN);
    }
}

int CliDispatcher::dispatch(const CliCommand& cmd)
{
    g_activeDispatcher = this;

    cancelToken_.reset();
    CliOutput::setQuiet(cmd.silent);
    CliOutput::setVerbose(cmd.verbose);
    CliOutput::setJsonMode(cmd.json);

    if (cmd.category == CliCategory::Help) {
        ArgParser().printHelp();
        g_activeDispatcher = nullptr;
        return 0;
    }
    if (cmd.category == CliCategory::Version) {
        ArgParser().printVersion();
        g_activeDispatcher = nullptr;
        return 0;
    }
    if (cmd.help) {
        ArgParser().printHelp(cmd.category);
        g_activeDispatcher = nullptr;
        return 0;
    }

    CLogger::Init("builder.log", "builder");

    int ret = 2;
    if (cmd.category == CliCategory::Info) {
        ret = dispatchInfo(cmd);
    } else if (cmd.category == CliCategory::Flow) {
        ret = dispatchFlow(cmd);
    } else if (cmd.category == CliCategory::Exec) {
        ret = dispatchExec(cmd);
    } else {
        CliOutput::error("Unknown command: '" + cmd.error
            + "'. Use --help for usage.");
    }

    g_activeDispatcher = nullptr;
    return ret;
}

int CliDispatcher::dispatchInfo(const CliCommand& cmd)
{
    if (cmd.verb == "version")      return cmdInfoVersion(cmd);
    if (cmd.verb == "system")       return cmdInfoSystem(cmd);
    if (cmd.verb == "git")          return cmdInfoGit(cmd);
    if (cmd.verb == "git-branches") return cmdListBranches(cmd);
    if (cmd.verb == "modpacks")     return cmdListModpacks(cmd);
    if (cmd.verb == "status")       return cmdStatus(cmd);
    if (cmd.verb == "workspace")    return cmdInfoWorkspace(cmd);
    if (cmd.verb == "preview")      return cmdInfoPreview(cmd);
    if (cmd.verb == "plugins")      return cmdInfoPlugins(cmd);
    if (cmd.verb == "exporters")    return cmdInfoExporters(cmd);
    if (cmd.verb == "pointers")     return cmdInfoPointers(cmd);
    if (cmd.verb == "history")      return cmdInfoHistory(cmd);
    if (cmd.verb == "debug")        return cmdInfoDebug(cmd);
    return notImplemented(cmd);
}

int CliDispatcher::dispatchFlow(const CliCommand& cmd)
{
    if (cmd.verb == "console") return cmdFlowConsole(cmd);
    return notImplemented(cmd);
}

int CliDispatcher::dispatchExec(const CliCommand& cmd)
{
    if (cmd.verb == "build")              return cmdBuild(cmd);
    if (cmd.verb == "export")             return cmdExport(cmd);
    if (cmd.verb == "sync-serverconfig")  return cmdSyncServerConfig(cmd);
    if (cmd.verb == "verify-repo")         return cmdVerifyRepo(cmd);
    if (cmd.verb == "resolve-pointer")     return cmdResolvePointer(cmd);
    if (cmd.verb == "crash-test")         return cmdCrashTest(cmd);
    if (cmd.verb == "git-update")         return cmdGitUpdate(cmd);
    return notImplemented(cmd);
}

void CliDispatcher::setGitConfig(const std::string& gitPath, bool useSystemGit)
{
    gitPath_ = gitPath;
    useSystemGit_ = useSystemGit;
}

int CliDispatcher::cmdCrashTest(const CliCommand& cmd)
{
    if (cmd.json) {
        CliOutput::jsonBlock({
            {"category", "exec"},
            {"command", "crash-test"},
            {"data", {
                {"message", "Crashing in 3 seconds to generate a crash report"},
                {"note", "Crash report will be written under the crash-report directory"}
            }}
        });
    }
    CLogger::Warn("CRASH TEST: 3...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    CLogger::Warn("CRASH TEST: 2...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    CLogger::Warn("CRASH TEST: 1...");
    std::this_thread::sleep_for(std::chrono::seconds(1));
    CLogger::Error("CRASH TEST: BOOM!");
    {
        volatile int* nullPtr = nullptr;
        *nullPtr = 0;
    }
    return 0;
}

int CliDispatcher::cmdGitUpdate(const CliCommand& cmd)
{
    if (useSystemGit_) {
        if (cmd.json) {
            CliOutput::jsonBlock({
                {"category", "exec"},
                {"command", "git-update"},
                {"data", {
                    {"installed", false},
                    {"use_system_git", true},
                    {"message", "System git mode — nothing to update"}
                }}
            });
        }
        CliOutput::warning("System git mode — nothing to update.");
        return 1;
    }

    CliOutput::title("Install Git");

    QString exeDirQ = QString::fromStdString(exeDir());
    GitInstallLayout layout = detectGitInstallLayout(exeDirQ);
    QString installRoot = layout.installRoot;
    QString gitDir = QDir(installRoot).absoluteFilePath("tools/git");
    QDir().mkpath(gitDir);
    QString gitExe = findRunnableGit(gitDir);
    QString currentVer = gitExe.isEmpty() ? QString() : installedGitVersion(gitExe);

    std::string installRootStr = QDir::toNativeSeparators(installRoot).toStdString();
    std::string gitExeStr = gitExe.isEmpty()
        ? "" : QDir::toNativeSeparators(gitExe).toStdString();

    if (!currentVer.isEmpty()) {
        CliOutput::info("Bundled Git already present: " + currentVer.toStdString());

        std::vector<std::string> confPaths = writeGitInstallConfFiles(layout.confPaths, installRootStr, gitExeStr);

        if (cmd.json) {
            nlohmann::json conf = nlohmann::json::array();
            for (auto& p : confPaths) conf.push_back(p);
            CliOutput::jsonBlock({
                {"category", "exec"},
                {"command", "git-update"},
                {"data", {
                    {"installed", true},
                    {"already_installed", true},
                    {"use_system_git", false},
                    {"install_root", installRootStr},
                    {"git_path", gitExeStr},
                    {"git_version", currentVer.toStdString()},
                    {"install_conf", conf}
                }}
            });
        } else {
            CliOutput::success("Git is ready at " + gitExeStr);
            CliOutput::info("Version: " + currentVer.toStdString());
            CliOutput::info("Install config written: install.conf");
        }
        return 0;
    }

    CliOutput::info("Installing bundled Git to " + gitDir.toStdString());

    QProcess ps;
    ps.start("powershell", {"-NoProfile", "-Command",
        "[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12;"
        "$r=Invoke-RestMethod -Uri 'https://api.github.com/repos/git-for-windows/git/releases/latest';"
        "foreach($a in $r.assets){if($a.name -like '*MinGit*64-bit*.zip'){"
        "Write-Output $a.browser_download_url;break}}"});
    ps.waitForFinished(30000);
    QString url = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
    if (url.isEmpty() || !url.startsWith("http")) {
        CliOutput::error("Failed to get Git URL");
        return 1;
    }

    CliOutput::info("Downloading " + url.toStdString());
    QString zip = QDir::tempPath() + "/neo_git_install.zip";
    {
        QProcess dl;
        dl.start("powershell", {"-Command", "Invoke-WebRequest", "-Uri", url, "-OutFile", zip});
        dl.waitForFinished(300000);
        if (dl.exitCode() != 0) {
            CliOutput::error("Download failed");
            return 1;
        }
    }

    QProcess tar;
    tar.setWorkingDirectory(gitDir);
    tar.start("tar", {"-xf", zip});
    tar.waitForFinished(120000);
    if (tar.exitCode() != 0) {
        QProcess ps2;
        ps2.start("powershell", {"-Command", "Expand-Archive", "-Path", zip,
            "-DestinationPath", gitDir, "-Force"});
        ps2.waitForFinished(120000);
    }
    QFile::remove(zip);

    gitExe = findRunnableGit(gitDir);
    currentVer = gitExe.isEmpty() ? QString() : installedGitVersion(gitExe);
    if (currentVer.isEmpty()) {
        CliOutput::error("Git install failed: no runnable git.exe found under " + gitDir.toStdString());
        return 1;
    }
    gitExeStr = QDir::toNativeSeparators(gitExe).toStdString();

    std::vector<std::string> confPaths = writeGitInstallConfFiles(layout.confPaths, installRootStr, gitExeStr);

    if (cmd.json) {
        nlohmann::json conf = nlohmann::json::array();
        for (auto& p : confPaths) conf.push_back(p);
        CliOutput::jsonBlock({
            {"category", "exec"},
            {"command", "git-update"},
            {"data", {
                {"installed", true},
                {"already_installed", false},
                {"use_system_git", false},
                {"install_root", installRootStr},
                {"git_path", gitExeStr},
                {"git_version", currentVer.toStdString()},
                {"install_conf", conf}
            }}
        });
    } else {
        CliOutput::success("Git installed to " + gitDir.toStdString());
        CliOutput::info("Version: " + currentVer.toStdString());
        CliOutput::info("Install config written: install.conf");
    }
    return 0;
}

int CliDispatcher::cmdVerifyRepo(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");

    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    CliOutput::title("Verify Repository");
    CliOutput::info("Repository: " + repoUrl);

    std::string workDir = ensureRepoCloned(repoUrl, cmd.get("git-branch"));
    if (workDir.empty()) return 1;

    NeoWorkspace::GitOperations gitOps;
    bool isRepo = gitOps.isGitRepository(workDir);
    std::string wsJson = findWorkspaceJson(workDir);
    bool hasWs = !wsJson.empty();

    auto branchResult = gitOps.listBranches(workDir);
    int branchCount = 0;
    {
        std::istringstream stream(branchResult.stdoutOutput);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty()) ++branchCount;
        }
    }

    auto revResult = gitOps.revParse(workDir, "HEAD");
    std::string head = revResult.exitCode == 0 ? revResult.stdoutOutput : "";
    {
        auto trimPos = head.find_last_not_of(" \t\r\n");
        if (trimPos == std::string::npos) head.clear();
        else head.erase(trimPos + 1);
    }

    nlohmann::json data;
    data["repo"] = repoUrl;
    data["workdir"] = workDir;
    data["is_git_repository"] = isRepo;
    data["has_workspace"] = hasWs;
    data["workspace_json"] = hasWs ? wsJson : "";
    data["branch_count"] = branchCount;
    data["head"] = head;
    data["valid"] = isRepo && hasWs;

    if (cmd.json) {
        CliOutput::jsonBlock({
            {"category", "exec"},
            {"command", "verify-repo"},
            {"data", data}
        });
    } else {
        CliOutput::info(std::string("Is git repository: ") + (isRepo ? "yes" : "no"));
        CliOutput::info(std::string("Has workspace.json: ") + (hasWs ? "yes" : "no"));
        CliOutput::info("Branch count: " + std::to_string(branchCount));
        if (!head.empty()) CliOutput::info("HEAD: " + head);
        if (isRepo && hasWs) {
            CliOutput::success("Repository is valid.");
        } else {
            CliOutput::error("Repository is invalid.");
        }
    }

    return (isRepo && hasWs) ? 0 : 1;
}

int CliDispatcher::cmdResolvePointer(const CliCommand& cmd)
{
    if (cmd.positional.empty()) {
        CliOutput::error("No pointer file specified. Usage: exec resolve-pointer <file.pointer>");
        return 2;
    }

    const std::string pointerPath = cmd.positional[0];
    CliOutput::title("Resolve Pointer");
    CliOutput::info("Pointer file: " + pointerPath);

    std::ifstream fin(pointerPath);
    if (!fin.is_open()) {
        CliOutput::error("Cannot open pointer file: " + pointerPath);
        return 1;
    }
    nlohmann::json root;
    try {
        fin >> root;
    } catch (const nlohmann::json::exception& e) {
        CliOutput::error("Invalid pointer JSON: " + std::string(e.what()));
        return 1;
    }

    NeoCore::PointerFileData pfd = NeoCore::PointerFileData::fromJson(root);
    if (pfd.sha256.empty()) {
        CliOutput::error("Pointer file missing 'sha256' field.");
        return 1;
    }
    if (pfd.resolvers.empty()) {
        CliOutput::error("Pointer file has no resolvers.");
        return 1;
    }

    NeoBuild::PointerDownloader downloader;
    downloader.scanResolvers((fs::path(exeDir()) / "pointers").string());

    NeoCore::PointerInfo pointer = pfd.resolvers.front();
    pointer.sha256 = pfd.sha256;

    auto resolved = downloader.resolveUrl(pointer);

    nlohmann::json data;
    data["file"] = pointerPath;
    data["sha256"] = pfd.sha256;
    data["resolver"] = resolved.resolver;
    data["url"] = resolved.success ? resolved.url : "";
    data["success"] = resolved.success;
    if (!resolved.success) data["error"] = resolved.errorMessage;

    if (cmd.json) {
        CliOutput::jsonBlock({
            {"category", "exec"},
            {"command", "resolve-pointer"},
            {"data", data}
        });
    } else if (resolved.success) {
        CliOutput::info("Resolver: " + resolved.resolver);
        CliOutput::info("URL: " + resolved.url);
        CliOutput::success("Pointer resolved.");
    } else {
        CliOutput::error("Failed to resolve pointer: " + resolved.errorMessage);
    }

    return resolved.success ? 0 : 1;
}

int CliDispatcher::notImplemented(const CliCommand& cmd)
{
    CliOutput::error("Command '" + ArgParser::categoryName(cmd.category)
        + " " + cmd.verb + "' is not implemented yet. See PLAN.md chapter 23.");
    return 2;
}

int CliDispatcher::cmdInfoVersion(const CliCommand& cmd)
{
    const std::string buildType =
#ifdef _DEBUG
        "debug"
#else
        "release"
#endif
        ;

    if (cmd.json) {
        CliOutput::jsonBlock({
            {"category", "info"},
            {"command", "version"},
            {"data", {
                {"version", ArgParser::version()},
                {"build_type", buildType}
            }}
        });
    } else {
        CliOutput::info("Version: " + ArgParser::version()
            + " (" + buildType + ")");
    }
    return 0;
}

std::string CliDispatcher::gitVersion() const
{
    QProcess ps;
    ps.start(QString::fromStdString(gitPath_), {"--version"});
    if (!ps.waitForFinished(5000)) return {};
    QString out = QString::fromUtf8(ps.readAllStandardOutput()).trimmed();
    if (out.isEmpty()) out = QString::fromUtf8(ps.readAllStandardError()).trimmed();
    return out.toStdString();
}

int CliDispatcher::cmdInfoSystem(const CliCommand& cmd)
{
    std::string exeDir = QCoreApplication::applicationDirPath().toStdString();
    nlohmann::json data;
    data["platform"] = QSysInfo::prettyProductName().toStdString();
    data["os"] = QSysInfo::productType().toStdString();
    data["kernel"] = QSysInfo::kernelVersion().toStdString();
    data["cpu_arch"] = QSysInfo::currentCpuArchitecture().toStdString();
    data["exe_dir"] = exeDir;
    data["data_dir"] = NeoBuild::getAppDataDir();
    data["cache_dir"] = NeoBuild::getCacheDir();
    data["config_dir"] = NeoBuild::getConfigDir();
    data["temp_dir"] = NeoBuild::getTempDir();
    data["default_workspace_dir"] = NeoBuild::getDefaultWorkspaceDir();
    uint64_t freeBytes = NeoBuild::getFreeDiskBytes(exeDir);
    data["workdir_free_bytes"] = freeBytes;
    data["git_available"] = NeoBuild::isGitAvailable();
    data["git_path"] = gitPath_;
    data["use_system_git"] = useSystemGit_;

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "system"}, {"data", data}});
    } else {
        CliOutput::title("System");
        CliOutput::info("Platform: " + data["platform"].get<std::string>());
        CliOutput::info("Kernel: " + data["kernel"].get<std::string>());
        CliOutput::info("CPU architecture: " + data["cpu_arch"].get<std::string>());
        CliOutput::info("Exe dir: " + exeDir);
        CliOutput::info("Data dir: " + data["data_dir"].get<std::string>());
        CliOutput::info("Cache dir: " + data["cache_dir"].get<std::string>());
        CliOutput::info("Config dir: " + data["config_dir"].get<std::string>());
        CliOutput::info("Temp dir: " + data["temp_dir"].get<std::string>());
        CliOutput::info("Default workspace: " + data["default_workspace_dir"].get<std::string>());
        CliOutput::info("Free disk: " + std::to_string(freeBytes) + " bytes");
        CliOutput::info("Git: " + data["git_path"].get<std::string>()
            + (useSystemGit_ ? " (system)" : " (bundled)"));
    }
    return 0;
}

int CliDispatcher::cmdInfoGit(const CliCommand& cmd)
{
    nlohmann::json data;
    data["git_path"] = gitPath_;
    data["use_system_git"] = useSystemGit_;
    data["git_version"] = gitVersion();

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "git"}, {"data", data}});
    } else {
        CliOutput::title("Git");
        CliOutput::info("Path: " + gitPath_
            + (useSystemGit_ ? " (system)" : " (bundled)"));
        if (!data["git_version"].get<std::string>().empty()) {
            CliOutput::info("Version: " + data["git_version"].get<std::string>());
        } else {
            CliOutput::warning("Could not determine git version.");
        }
    }
    return 0;
}

static nlohmann::json branchConfigToJson(const NeoWorkspace::WorkspaceManager::BranchConfig& b)
{
    nlohmann::json j;
    j["name"] = b.name;
    j["parent"] = b.parent;
    j["game_version"] = b.gameVersion;
    j["modloader"] = b.modloader;
    j["modloader_version"] = b.modloaderVersion;
    j["hidden"] = b.hidden;
    j["description"] = b.description;
    return j;
}

int CliDispatcher::cmdInfoWorkspace(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, cmd.get("git-branch"));
    if (workDir.empty()) return 1;

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found in repository root.");
        return 1;
    }

    NeoWorkspace::WorkspaceManager wm;
    if (!wm.loadFromFile(wsJson)) {
        CliOutput::error("Failed to parse workspace.json.");
        return 1;
    }

    nlohmann::json data;
    data["workdir"] = workDir;
    data["name"] = wm.workspaceName();
    data["minecraft_version"] = wm.minecraftVersion();
    data["modloader"] = wm.modloader();
    data["git_remote"] = wm.gitRemote();
    data["default_branch"] = wm.defaultBranch();

    nlohmann::json branchesArr = nlohmann::json::array();
    for (const auto& b : wm.branches()) {
        auto obj = branchConfigToJson(b);
        obj["inheritance_chain"] = wm.branchInheritanceChain(b.name);
        branchesArr.push_back(obj);
    }
    data["branches"] = branchesArr;

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "workspace"}, {"data", data}});
    } else {
        CliOutput::title("Workspace");
        CliOutput::info("Name: " + wm.workspaceName());
        CliOutput::info("Minecraft: " + wm.minecraftVersion());
        CliOutput::info("Modloader: " + wm.modloader());
        CliOutput::info("Remote: " + wm.gitRemote());
        CliOutput::info("Default branch: " + wm.defaultBranch());
        CliOutput::info(std::to_string(wm.branches().size()) + " branch(es).");
        for (const auto& b : wm.branches()) {
            auto chain = wm.branchInheritanceChain(b.name);
            std::string chainStr;
            for (size_t i = 0; i < chain.size(); ++i) {
                if (i) chainStr += " -> ";
                chainStr += chain[i];
            }
            CliOutput::info("  " + b.name + " [parent="
                + (b.parent.empty() ? "(root)" : b.parent) + "]"
                + (chainStr.empty() ? "" : " chain=" + chainStr));
        }
    }
    return 0;
}

int CliDispatcher::cmdInfoPreview(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    const std::string modpackBranch = cmd.get("modpack");
    const std::string fmt = cmd.get("format");

    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }
    if (modpackBranch.empty()) {
        CliOutput::error("No modpack branch specified. Use --modpack <branch>.");
        return 2;
    }

    std::string format = fmt.empty() ? "mcbbs" : fmt;
    if (format != "mcbbs" && format != "modrinth" && format != "hmcl") {
        CliOutput::error("Unsupported format: " + format + ". Use mcbbs, modrinth, or hmcl.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, cmd.get("git-branch"));
    if (workDir.empty()) return 1;

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found in repository root.");
        return 1;
    }

    std::string previewOut = (fs::temp_directory_path() / "nsum_preview").string();
    NeoBuild::BuildEngine engine;
    engine.setGitPath(gitPath_);
    if (!engine.init(workDir, NeoBuild::getCacheDir(), previewOut)) {
        CliOutput::error("Failed to initialize build engine.");
        return 1;
    }

    CliOutput::info("Running virtual build for preview: " + modpackBranch);
    auto result = engine.build(modpackBranch, nullptr, &cancelToken_);
    if (!result.success) {
        CliOutput::error("Preview build failed: " + result.errorMessage);
        return 1;
    }

    NeoCore::ExportMetadata meta;
    fillExportMetadata(meta, wsJson);

    NeoBuild::ModpackExporter exporter;
    exporter.scanExporters((fs::path(exeDir()) / "exporters").string());
    auto tree = exporter.previewStructure(format, result.outputDir, meta);

    nlohmann::json data;
    data["format"] = format;
    data["branch"] = modpackBranch;
    data["entries"] = tree;

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "preview"}, {"data", data}});
    } else {
        CliOutput::title("Preview");
        CliOutput::info("Format: " + format);
        CliOutput::info("Branch: " + modpackBranch);
        CliOutput::info(std::to_string(tree.size()) + " entries.");
    }
    return 0;
}

static nlohmann::json scanMetaDir(const std::string& dir)
{
    nlohmann::json arr = nlohmann::json::array();
    std::error_code ec;
    if (!fs::exists(dir) || !fs::is_directory(dir)) return arr;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (ext != ".json") continue;
        std::ifstream f(entry.path());
        if (!f.is_open()) continue;
        try {
            nlohmann::json meta = nlohmann::json::parse(f);
            meta["file"] = entry.path().filename().string();
            arr.push_back(meta);
        } catch (const std::exception&) {
        }
    }
    return arr;
}

int CliDispatcher::cmdInfoPlugins(const CliCommand& cmd)
{
    std::string base = exeDir();
    nlohmann::json data;
    data["parsers"] = scanMetaDir((fs::path(base) / "parsers").string());
    data["pointers"] = scanMetaDir((fs::path(base) / "pointers").string());
    data["exporters"] = scanMetaDir((fs::path(base) / "exporters").string());
    data["parser_count"] = data["parsers"].size();
    data["pointer_count"] = data["pointers"].size();
    data["exporter_count"] = data["exporters"].size();

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "plugins"}, {"data", data}});
    } else {
        CliOutput::title("Plugins");
        CliOutput::info("Parsers: " + std::to_string(data["parser_count"].get<size_t>()));
        CliOutput::info("Pointers: " + std::to_string(data["pointer_count"].get<size_t>()));
        CliOutput::info("Exporters: " + std::to_string(data["exporter_count"].get<size_t>()));
        for (const auto& p : data["parsers"]) {
            CliOutput::info("  [parser] " + p.value("name", ""));
        }
        for (const auto& p : data["pointers"]) {
            CliOutput::info("  [pointer] " + p.value("name", ""));
        }
        for (const auto& p : data["exporters"]) {
            CliOutput::info("  [exporter] " + p.value("name", ""));
        }
    }
    return 0;
}

int CliDispatcher::cmdInfoExporters(const CliCommand& cmd)
{
    auto arr = scanMetaDir((fs::path(exeDir()) / "exporters").string());
    nlohmann::json data;
    data["exporters"] = arr;
    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "exporters"}, {"data", data}});
    } else {
        CliOutput::title("Exporters");
        for (const auto& e : arr) {
            std::string line = e.value("format", "") + " (" + e.value("extension", "") + ") - "
                + e.value("name", "");
            if (e.contains("fields") && e["fields"].is_array()) {
                line += " [" + std::to_string(e["fields"].size()) + " fields]";
            }
            CliOutput::info(line);
        }
    }
    return 0;
}

namespace {

struct ConsoleField {
    std::string key;
    std::string label;
    std::string type;
    bool required = false;
};

struct FlowConsoleData {
    std::string repo;
    std::string repoLocalPath;
    std::string branch;
    std::string modpack;
    std::string format;
    std::string exportOutput;
    std::map<std::string, std::string> extra;
};

static std::string trimStr(std::string s)
{
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        [&](unsigned char c) { return !isSpace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
        [&](unsigned char c) { return !isSpace(c); }).base(), s.end());
    return s;
}

static int flowPageIndex(const std::string& name)
{
    static const std::vector<std::string> names = {
        "repo", "branch", "modpack", "export-type", "export-dir",
        "extra-info", "checklist", "build", "done"};
    for (size_t i = 0; i < names.size(); ++i) {
        if (name == names[i]) return static_cast<int>(i);
    }
    return -1;
}

// Human/prompt output: stderr in --json mode so stdout stays machine-readable.
static void consolePrint(const std::string& text)
{
    if (CliOutput::isJsonMode()) {
        std::cerr << text;
        std::cerr.flush();
    } else {
        std::cout << text;
        std::cout.flush();
    }
}

// Read one line from stdin. Returns false on EOF (cancel).
static bool readConsoleLine(std::string& out)
{
    std::cout.flush();
    std::cerr.flush();
    if (!std::getline(std::cin, out)) return false;
    return true;
}

// Numbered choice prompt; loops until a valid pick or empty line (cancel).
// Returns true and sets out on success; false on cancel.
static bool promptConsoleChoice(const std::string& title,
    const std::vector<std::string>& options, std::string& out)
{
    for (;;) {
        consolePrint(title + ":\n");
        for (size_t i = 0; i < options.size(); ++i) {
            consolePrint("  [" + std::to_string(i + 1) + "] " + options[i] + "\n");
        }
        consolePrint("  [q] cancel\n");
        consolePrint("> ");
        std::string line;
        if (!readConsoleLine(line)) return false;
        std::string t = trimStr(line);
        if (t.empty() || t == "q" || t == "quit" || t == "cancel") return false;
        try {
            int n = std::stoi(t);
            if (n >= 1 && n <= static_cast<int>(options.size())) {
                out = options[n - 1];
                return true;
            }
        } catch (...) {
        }
        for (const auto& o : options) {
            if (o == t) { out = o; return true; }
        }
        CliOutput::warning("Invalid selection: " + t);
    }
}

// Text prompt; empty input is rejected unless allowEmpty. False on cancel.
static bool promptConsoleText(const std::string& prompt,
    bool allowEmpty, std::string& out)
{
    for (;;) {
        consolePrint(prompt + "> ");
        std::string line;
        if (!readConsoleLine(line)) {
            if (allowEmpty) { out.clear(); return true; }
            return false;
        }
        std::string t = trimStr(line);
        if (t.empty()) {
            if (allowEmpty) { out.clear(); return true; }
            consolePrint("(empty not allowed, enter value or 'q' to cancel)\n");
            continue;
        }
        if (t == "q" || t == "quit" || t == "cancel") return false;
        out = t;
        return true;
    }
}

static std::vector<std::string> listExportFormats()
{
    std::vector<std::string> out;
    auto arr = scanMetaDir((fs::path(exeDir()) / "exporters").string());
    for (const auto& e : arr) {
        std::string fmt = e.value("format", "");
        std::string ext = e.value("extension", "");
        if (fmt.empty()) continue;
        out.push_back(ext.empty() ? fmt : (fmt + " (" + ext + ")"));
    }
    std::sort(out.begin(), out.end());
    return out;
}

static std::string formatKeyFromDisplay(const std::string& display)
{
    auto pos = display.find(' ');
    if (pos == std::string::npos) return display;
    std::string ext = trimStr(display.substr(pos + 1));
    if (ext.size() > 1 && ext.front() == '(' && ext.back() == ')') {
        ext = ext.substr(1, ext.size() - 2);
    }
    return ext == ".mrpack" ? "modrinth" : (ext == ".zip" ? "mcbbs" : "hmcl");
}

static std::vector<ConsoleField> exporterFieldsFor(const std::string& formatId)
{
    std::vector<ConsoleField> out;
    auto arr = scanMetaDir((fs::path(exeDir()) / "exporters").string());
    for (const auto& e : arr) {
        if (e.value("format", "") != formatId) continue;
        if (!e.contains("fields") || !e["fields"].is_array()) return out;
        for (const auto& f : e["fields"]) {
            ConsoleField cf;
            cf.key = f.value("key", "");
            cf.label = f.value("label", "");
            cf.type = f.value("type", "");
            cf.required = f.value("required", false);
            if (!cf.key.empty()) out.push_back(cf);
        }
        break;
    }
    return out;
}

} // namespace

int CliDispatcher::cmdFlowConsole(const CliCommand& cmd)
{
    int startIndex = 0;
    int endIndex = 6; // checklist: collect data, never build
    if (cmd.has("to")) {
        int idx = flowPageIndex(cmd.get("to"));
        if (idx < 0) {
            CliOutput::error("Invalid --to page: '" + cmd.get("to")
                + "'. Use --help for usage.");
            return 2;
        }
        if (idx > 6) {
            CliOutput::error("--to '" + cmd.get("to")
                + "' is not supported by console (max is 'checklist'). "
                "Use --help for usage.");
            return 2;
        }
        endIndex = idx;
    }
    if (cmd.has("from")) {
        int idx = flowPageIndex(cmd.get("from"));
        if (idx < 0) {
            CliOutput::error("Invalid --from page: '" + cmd.get("from")
                + "'. Use --help for usage.");
            return 2;
        }
        if (idx > 6) {
            CliOutput::error("--from '" + cmd.get("from")
                + "' is not supported (max build). Use --help for usage.");
            return 2;
        }
        startIndex = idx;
    }
    if (startIndex > endIndex) startIndex = endIndex;

    std::map<std::string, std::string> prefill;
    for (const auto& kv : cmd.prefill) {
        auto eq = kv.find('=');
        if (eq == std::string::npos) {
            CliOutput::error("Invalid --prefill '" + kv
                + "'. Expected key=value. Use --help for usage.");
            return 2;
        }
        std::string k = trimStr(kv.substr(0, eq));
        std::string v = kv.substr(eq + 1);
        if (k.empty() || v.empty()) {
            CliOutput::error("Invalid --prefill '" + kv
                + "'. Expected key=value. Use --help for usage.");
            return 2;
        }
        prefill[k] = v;
    }

    CliOutput::title("flow console");

    FlowConsoleData d;
    bool cancelled = false;

    auto cancelledOut = [&]() -> int {
        if (cancelled) {
            std::cerr << "Flow cancelled." << std::endl;
            return 1;
        }
        return 0;
    };

    // repo page (dependency for branch/modpack)
    if (endIndex >= 1 || startIndex == 0) {
        if (prefill.count("repo")) {
            d.repo = prefill["repo"];
            CliOutput::info("Using prefilled repo: " + d.repo);
        } else if (startIndex <= 0) {
            std::string v;
            CliOutput::info("Repo — Git repository URL or local path:");
            if (!promptConsoleText("repo", false, v)) { cancelled = true; return cancelledOut(); }
            d.repo = v;
        } else {
            CliOutput::error("repo is required before the branch page. "
                "Pass --prefill repo=<url|path>.");
            return 2;
        }
        if (d.repo.rfind("file://", 0) == 0) {
            d.repo = QUrl(QString::fromStdString(d.repo)).toLocalFile().toStdString();
        }
        if (fs::exists(fs::path(d.repo) / ".git")) {
            d.repoLocalPath = d.repo;
        } else if (d.repo.rfind("http", 0) == 0 || d.repo.rfind("git@", 0) == 0
            || d.repo.rfind("ssh://", 0) == 0) {
            std::string wd = ensureRepoCloned(d.repo, "");
            if (wd.empty()) return 1;
            d.repoLocalPath = wd;
        } else if (fs::exists(d.repo)) {
            d.repoLocalPath = d.repo;
        } else {
            CliOutput::error("Repository not found: " + d.repo);
            return 1;
        }
    }

    // branch page
    if (endIndex >= 1) {
        if (prefill.count("branch")) {
            d.branch = prefill["branch"];
            CliOutput::info("Using prefilled branch: " + d.branch);
        } else {
            NeoWorkspace::GitOperations gitOps;
            auto result = gitOps.listBranches(d.repoLocalPath);
            if (result.exitCode != 0) result = gitOps.listRemoteBranches(d.repoLocalPath);
            std::vector<std::string> branches;
            std::istringstream stream(result.stdoutOutput);
            std::string line;
            while (std::getline(stream, line)) {
                while (!line.empty() && (line[0] == ' ' || line[0] == '*'))
                    line.erase(0, 1);
                if (line.empty()) continue;
                if (line.find("->") != std::string::npos) continue;
                if (line.find("origin/") == 0) line.erase(0, 7);
                if (line.find("remotes/") == 0) {
                    auto pos = line.find('/', 8);
                    if (pos != std::string::npos) line = line.substr(pos + 1);
                }
                branches.push_back(line);
            }
            std::sort(branches.begin(), branches.end());
            branches.erase(std::unique(branches.begin(), branches.end()), branches.end());
            if (branches.empty()) {
                CliOutput::error("No branches found in repository.");
                return 1;
            }
            if (!promptConsoleChoice("Select branch", branches, d.branch)) {
                cancelled = true;
                return cancelledOut();
            }
        }
    }

    // modpack page
    if (endIndex >= 2) {
        std::string wsJson = findWorkspaceJson(d.repoLocalPath);
        if (wsJson.empty()) {
            CliOutput::error("workspace.json not found in repository root.");
            return 1;
        }
        NeoWorkspace::WorkspaceManager wm;
        if (!wm.loadFromFile(wsJson)) {
            CliOutput::error("Failed to parse workspace.json.");
            return 1;
        }
        std::vector<std::string> modpacks;
        for (const auto& b : wm.branches()) {
            if (b.hidden) continue;
            std::string desc = b.description.empty()
                ? std::string() : (" - " + b.description);
            modpacks.push_back(b.name + desc);
        }
        if (prefill.count("modpack")) {
            d.modpack = prefill["modpack"];
            CliOutput::info("Using prefilled modpack: " + d.modpack);
        } else {
            if (modpacks.empty()) {
                CliOutput::error("No modpack branches defined in workspace.json.");
                return 1;
            }
            std::string display;
            if (!promptConsoleChoice("Select modpack", modpacks, display)) {
                cancelled = true;
                return cancelledOut();
            }
            auto space = display.find(' ');
            d.modpack = space == std::string::npos ? display : display.substr(0, space);
        }
    }

    // export-type page
    if (endIndex >= 3) {
        if (prefill.count("format")) {
            d.format = prefill["format"];
            CliOutput::info("Using prefilled format: " + d.format);
        } else {
            auto formats = listExportFormats();
            if (formats.empty()) {
                CliOutput::error("No exporter plugins found in exporters/.");
                return 1;
            }
            std::string display;
            if (!promptConsoleChoice("Select export format", formats, display)) {
                cancelled = true;
                return cancelledOut();
            }
            d.format = formatKeyFromDisplay(display);
        }
    }

    // export-dir page
    if (endIndex >= 4) {
        std::string dir;
        if (prefill.count("exportdir")) {
            dir = prefill["exportdir"];
            CliOutput::info("Using prefilled export dir: " + dir);
        } else {
            CliOutput::info("Export dir — directory where the modpack is written.");
            if (!promptConsoleText("export-dir", false, dir)) {
                cancelled = true;
                return cancelledOut();
            }
        }
        std::string ext = d.format == "modrinth" ? ".mrpack" : ".zip";
        std::string base = d.modpack.empty() ? "modpack" : d.modpack;
        if (d.format == "hmcl") {
            d.exportOutput = dir;
        } else {
            d.exportOutput = dir + "/" + base + "_modpack" + ext;
        }
    }

    // extra-info page
    if (endIndex >= 5) {
        auto fields = exporterFieldsFor(d.format);
        for (const auto& f : fields) {
            std::string val;
            if (prefill.count(f.key)) {
                val = prefill[f.key];
                CliOutput::info("Using prefilled " + f.key + ": " + val);
            } else {
                std::string label = f.label.empty() ? f.key : f.label;
                std::string v;
                if (!promptConsoleText(label, !f.required, v)) {
                    cancelled = true;
                    return cancelledOut();
                }
                val = v;
            }
            if (!val.empty()) d.extra[f.key] = val;
        }
    }

    json data;
    data["repo"] = d.repo;
    data["repo_local_path"] = d.repoLocalPath;
    data["branch"] = d.branch;
    data["modpack"] = d.modpack;
    data["format"] = d.format;
    data["export_dir"] = d.exportOutput;
    if (!d.extra.empty()) {
        json extra = json::object();
        for (const auto& kv : d.extra) extra[kv.first] = kv.second;
        data["extra"] = extra;
    }
    CliOutput::jsonBlock({{"category", "flow"}, {"command", "console"}, {"data", data}});
    return 0;
}

int CliDispatcher::cmdInfoPointers(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, cmd.get("git-branch"));
    if (workDir.empty()) return 1;

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found.");
        return 1;
    }

    NeoWorkspace::WorkspaceManager wm;
    if (!wm.loadFromFile(wsJson)) {
        CliOutput::error("Failed to parse workspace.json.");
        return 1;
    }

    auto ptrs = wm.pointerFiles();
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& [sha, info] : ptrs) {
        nlohmann::json e;
        e["sha256"] = sha;
        e["resolver"] = info.resolver;
        if (info.metadata.is_object()) e["metadata"] = info.metadata;
        arr.push_back(e);
    }

    nlohmann::json data;
    data["count"] = static_cast<int>(ptrs.size());
    data["pointers"] = arr;

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "pointers"}, {"data", data}});
    } else {
        CliOutput::title("Pointers");
        CliOutput::info(std::to_string(ptrs.size()) + " pointer(s).");
        for (const auto& [sha, info] : ptrs) {
            CliOutput::info("  " + sha.substr(0, 12) + "..." + " resolver=" + info.resolver);
        }
    }
    return 0;
}

int CliDispatcher::cmdInfoHistory(const CliCommand& cmd)
{
    std::string typeFilter = cmd.get("type");
    auto stored = NeoWorkspace::HistoryStore::readRecentRepos();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& e : stored) {
        std::string tname;
        int tier = static_cast<int>(e.type);
        if (tier == static_cast<int>(NeoWorkspace::RepoType::Local)) tname = "local";
        else if (tier == static_cast<int>(NeoWorkspace::RepoType::Cache)) tname = "cache";
        else tname = "remote";

        if (!typeFilter.empty() && typeFilter != tname) continue;

        nlohmann::json obj;
        obj["type"] = tname;
        obj["location"] = e.location;
        if (!e.cachePath.empty()) obj["cache_path"] = e.cachePath;
        arr.push_back(obj);
    }

    nlohmann::json data;
    data["count"] = arr.size();
    data["history_dir"] = NeoWorkspace::HistoryStore::historyDir();
    data["recent_cache_dir"] = NeoWorkspace::HistoryStore::recentCacheDir();
    data["entries"] = arr;

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "history"}, {"data", data}});
    } else {
        CliOutput::title("History");
        CliOutput::info(std::to_string(arr.size()) + " recent repo(s).");
        for (const auto& e : arr) {
            CliOutput::info("  [" + e["type"].get<std::string>()
                + "] " + e["location"].get<std::string>());
        }
    }
    return 0;
}

int CliDispatcher::cmdInfoDebug(const CliCommand& cmd)
{
    nlohmann::json data;
    const std::string buildType =
#ifdef _DEBUG
        "debug"
#else
        "release"
#endif
        ;
    data["version"] = ArgParser::version();
    data["build_type"] = buildType;
    data["platform"] = QSysInfo::prettyProductName().toStdString();
    data["os"] = QSysInfo::productType().toStdString();
    data["cpu_arch"] = QSysInfo::currentCpuArchitecture().toStdString();
    data["exe_dir"] = QCoreApplication::applicationDirPath().toStdString();
    data["data_dir"] = NeoBuild::getAppDataDir();
    data["cache_dir"] = NeoBuild::getCacheDir();
    data["config_dir"] = NeoBuild::getConfigDir();
    data["temp_dir"] = NeoBuild::getTempDir();
    data["git_path"] = gitPath_;
    data["use_system_git"] = useSystemGit_;
    data["git_version"] = gitVersion();
    data["parser_count"] = scanMetaDir((fs::path(exeDir()) / "parsers").string()).size();
    data["pointer_count"] = scanMetaDir((fs::path(exeDir()) / "pointers").string()).size();
    data["exporter_count"] = scanMetaDir((fs::path(exeDir()) / "exporters").string()).size();

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "debug"}, {"data", data}});
    } else {
        CliOutput::title("Debug");
        CliOutput::info("Version: " + ArgParser::version() + " (" + buildType + ")");
        CliOutput::info("OS: " + data["platform"].get<std::string>());
        CliOutput::info("CPU architecture: " + data["cpu_arch"].get<std::string>());
        CliOutput::info("Exe dir: " + data["exe_dir"].get<std::string>());
        CliOutput::info("Git: " + data["git_path"].get<std::string>()
            + (useSystemGit_ ? " (system)" : " (bundled)"));
        if (!data["git_version"].get<std::string>().empty()) {
            CliOutput::info("Git version: " + data["git_version"].get<std::string>());
        }
        CliOutput::info("Parsers: " + std::to_string(data["parser_count"].get<size_t>())
            + ", Pointers: " + std::to_string(data["pointer_count"].get<size_t>())
            + ", Exporters: " + std::to_string(data["exporter_count"].get<size_t>()));
    }
    return 0;
}

bool CliDispatcher::isCancelled() const { return cancelToken_.is_cancelled(); }
void CliDispatcher::cancel() { cancelToken_.request_cancel(); }

std::string CliDispatcher::resolveWorkDir(const std::string& repoUrl) const
{
    std::string base = NeoBuild::getCacheDir();
    if (base.empty()) {
        base = fs::temp_directory_path().string();
    }
    fs::path dir = fs::path(base) / "repos" / slugifyRepo(repoUrl);
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir.string();
}

std::string CliDispatcher::ensureRepoCloned(const std::string& repoUrl,
    const std::string& gitBranch)
{
    std::string workDir = resolveWorkDir(repoUrl);
    NeoWorkspace::GitOperations gitOps;

    bool isRepo = gitOps.isGitRepository(workDir);

    if (!isRepo) {
        CliOutput::info("Cloning repository: " + repoUrl);
        if (cancelToken_.is_cancelled()) return {};
        auto result = gitOps.clone(repoUrl, workDir, 300000);
        if (result.exitCode != 0) {
            CliOutput::error("Failed to clone repository: " + result.stderrOutput);
            CLogger::Error("Clone failed for {}: {}", repoUrl, result.stderrOutput);
            return {};
        }
        CliOutput::success("Repository cloned.");
    } else {
        CliOutput::info("Fetching latest changes...");
        if (cancelToken_.is_cancelled()) return {};
        auto result = gitOps.fetch(workDir);
        if (result.exitCode != 0) {
            CliOutput::warning("Failed to fetch: " + result.stderrOutput);
        }
    }

    if (!gitBranch.empty()) {
        CliOutput::info("Checking out branch: " + gitBranch);
        if (cancelToken_.is_cancelled()) return {};
        auto result = gitOps.checkout(workDir, gitBranch);
        if (result.exitCode != 0) {
            CliOutput::error("Failed to checkout branch " + gitBranch + ": " + result.stderrOutput);
            return {};
        }
    }

    return workDir;
}

std::string CliDispatcher::findWorkspaceJson(const std::string& workDir) const
{
    fs::path wsPath = fs::path(workDir) / "workspace.json";
    if (fs::exists(wsPath)) {
        return wsPath.string();
    }
    return {};
}

std::string CliDispatcher::findBuildOutputDir(const std::string& workDir,
    const std::string& modpackBranch) const
{
    // sync_target 已废弃 (2026-08-05)，输出目录固定 .minecraft/versions/<branch>
    return (fs::path(workDir) / ".minecraft/versions" / modpackBranch).string();
}

int CliDispatcher::cmdBuild(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    const std::string modpackBranch = cmd.get("modpack");
    const std::string gitBranch = cmd.get("git-branch");
    const std::string exportFormat = cmd.get("format");
    const std::string exportPath = cmd.get("export");

    CliOutput::title("Build Modpack");
    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }
    if (modpackBranch.empty()) {
        CliOutput::error("No modpack branch specified. Use --modpack <branch>.");
        return 2;
    }
    CliOutput::info("Repository: " + repoUrl);
    CliOutput::info("Modpack branch: " + modpackBranch);
    if (!gitBranch.empty())
        CliOutput::info("Git branch: " + gitBranch);

    std::string fmt = exportFormat.empty() ? "mcbbs" : exportFormat;
    if (fmt != "mcbbs" && fmt != "modrinth" && fmt != "hmcl") {
        CliOutput::error("Unsupported format: " + fmt + ". Use mcbbs, modrinth, or hmcl.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, gitBranch);
    if (workDir.empty()) return 1;

    if (cancelToken_.is_cancelled()) {
        CliOutput::warning("Build cancelled by user.");
        return 0;
    }

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found in repository root.");
        return 1;
    }

    NeoBuild::ModpackExporter exporter;
    exporter.scanExporters((fs::path(exeDir()) / "exporters").string());
    NeoCore::IModpackExporter* plugin = exporter.exporterForFormat(fmt);
    if (!plugin) {
        CliOutput::error("Exporter plugin not found for format: " + fmt);
        return 1;
    }

    NeoCore::BuildTarget target;
    target.workspace_path = workDir;
    target.workspace_json = wsJson;
    target.cache_dir = NeoBuild::getCacheDir();
    target.branch = modpackBranch;
    target.sync_to_directory = (fmt == "hmcl");
    fillExportMetadata(target.metadata, wsJson);

    if (target.sync_to_directory) {
        if (!exportPath.empty()) {
            target.output_path = exportPath;
        } else {
            target.output_path = findBuildOutputDir(workDir, modpackBranch);
        }
        if (target.output_path.empty()) {
            CliOutput::error("Cannot resolve sync target directory from workspace.json.");
            return 1;
        }
    } else {
        target.output_path = (fs::path(workDir) / ".minecraft" / "versions").string();
    }

    CliBuildProgress progress(&cancelToken_);
    CliOutput::info("Starting build...");
    auto result = plugin->build_modpack(target, &progress, &cancelToken_);

    if (cancelToken_.is_cancelled()) {
        CliOutput::warning("Build cancelled by user.");
        return 0;
    }

    if (!result.success) {
        CliOutput::error("Build failed: " + result.errorMessage);
        CLogger::Error("Build failed for {}: {}",
            modpackBranch, result.errorMessage);
        return 1;
    }

    CliOutput::success("Build completed successfully.");
    CliOutput::info("Output directory: " + result.outputDir);
    CliOutput::info(std::to_string(result.syncedFiles) + " files synced, "
        + std::to_string(result.failedFiles) + " failed.");

    for (const auto& w : result.warnings) {
        CliOutput::warning(w);
    }

    if (!target.sync_to_directory && !exportPath.empty()) {
        CliOutput::info("Exporting to: " + exportPath);
        if (!plugin->export_modpack(result.outputDir, exportPath, target.metadata)) {
            CliOutput::error("Export failed.");
            return 1;
        }
        CliOutput::success("Exported to: " + exportPath);
    }

    return 0;
}

int CliDispatcher::cmdExport(const CliCommand& cmd)
{
    const std::string exportPath = cmd.get("export");
    const std::string exportFormat = cmd.get("format");
    const std::string repoUrl = cmd.get("repo");
    const std::string modpackBranch = cmd.get("modpack");
    const std::string gitBranch = cmd.get("git-branch");

    CliOutput::title("Export Modpack");

    if (exportPath.empty()) {
        CliOutput::error("No export path specified. Use --export <path>.");
        return 2;
    }
    if (exportFormat.empty()) {
        CliOutput::error("No export format specified. Use --format <mcbbs|modrinth|hmcl>.");
        return 2;
    }

    std::string fmt = exportFormat;
    if (fmt != "mcbbs" && fmt != "modrinth" && fmt != "hmcl") {
        CliOutput::error("Unsupported format: " + fmt + ". Use mcbbs, modrinth, or hmcl.");
        return 2;
    }

    std::string workDir;
    std::string buildOutputDir;

    if (!repoUrl.empty() && !modpackBranch.empty()) {
        workDir = ensureRepoCloned(repoUrl, gitBranch);
        if (workDir.empty()) return 1;

        if (cancelToken_.is_cancelled()) {
            CliOutput::warning("Export cancelled by user.");
            return 0;
        }

        std::string wsJson = findWorkspaceJson(workDir);
        if (wsJson.empty()) {
            CliOutput::error("workspace.json not found in repository root.");
            return 1;
        }

        NeoBuild::ModpackExporter exporter;
        exporter.scanExporters((fs::path(exeDir()) / "exporters").string());
        NeoCore::IModpackExporter* plugin = exporter.exporterForFormat(fmt);
        if (!plugin) {
            CliOutput::error("Exporter plugin not found for format: " + fmt);
            return 1;
        }

        NeoCore::BuildTarget target;
        target.workspace_path = workDir;
        target.workspace_json = wsJson;
        target.cache_dir = NeoBuild::getCacheDir();
        target.branch = modpackBranch;
        target.sync_to_directory = (fmt == "hmcl");
        fillExportMetadata(target.metadata, wsJson);

        if (target.sync_to_directory) {
            if (!exportPath.empty()) {
                target.output_path = exportPath;
            } else {
                target.output_path = findBuildOutputDir(workDir, modpackBranch);
            }
            if (target.output_path.empty()) {
                CliOutput::error("Cannot resolve sync target directory from workspace.json.");
                return 1;
            }
        } else {
            target.output_path = (fs::path(workDir) / ".minecraft" / "versions").string();
        }

        CliBuildProgress progress(&cancelToken_);
        CliOutput::info("Building before export...");
        auto buildResult = plugin->build_modpack(target, &progress, &cancelToken_);

        if (cancelToken_.is_cancelled()) {
            CliOutput::warning("Export cancelled by user.");
            return 0;
        }

        if (!buildResult.success) {
            CliOutput::error("Build failed: " + buildResult.errorMessage);
            return 1;
        }

        buildOutputDir = buildResult.outputDir;
        CliOutput::success("Build complete. Exporting...");
    } else {
        CliOutput::info("Standalone export mode. Searching for workspace...");

        if (!repoUrl.empty()) {
            workDir = resolveWorkDir(repoUrl);
        } else if (fs::exists(fs::current_path() / "workspace.json")) {
            workDir = fs::current_path().string();
        } else if (fs::exists(fs::current_path().parent_path() / "workspace.json")) {
            workDir = fs::current_path().parent_path().string();
        }

        if (workDir.empty()) {
            CliOutput::error(
                "No workspace found. Provide --repo <url> or run from a workspace directory.");
            return 1;
        }

        if (!modpackBranch.empty()) {
            buildOutputDir = findBuildOutputDir(workDir, modpackBranch);
        } else {
            CliOutput::warning("No --modpack specified. Searching for any built output...");
            fs::path versionsDir = fs::path(workDir) / ".minecraft" / "versions";
            if (fs::exists(versionsDir)) {
                for (const auto& entry : fs::directory_iterator(versionsDir)) {
                    if (entry.is_directory()
                        && entry.path().filename().string().find(".cache") == std::string::npos
                        && fs::exists(entry.path() / "mods")) {
                        buildOutputDir = entry.path().string();
                        break;
                    }
                }
            }
        }

        if (buildOutputDir.empty() || !fs::exists(buildOutputDir)) {
            CliOutput::error("No previously-built output found. Build first with --repo --modpack.");
            return 1;
        }

        CliOutput::info("Using workspace: " + workDir);
        CliOutput::info("Using build output: " + buildOutputDir);
    }

    NeoBuild::ModpackExporter exporter;
    std::string exportersDir = (fs::path(exeDir()) / "exporters").string();
    exporter.scanExporters(exportersDir);

    NeoCore::ExportMetadata meta;
    meta.name = "NeoServer";
    meta.version = "1.0";
    meta.game_version = "1.21.4";

    if (!workDir.empty()) {
        fillExportMetadata(meta, findWorkspaceJson(workDir));
    }

    if (fmt == "hmcl") {
        CliOutput::success("Workspace sync completed: " + buildOutputDir);
        return 0;
    }

    if (!exporter.exportModpack(fmt, buildOutputDir, exportPath, meta, &cancelToken_)) {
        CliOutput::error("Export failed for format: " + fmt);
        return 1;
    }

    CliOutput::success("Export completed: " + exportPath);
    return 0;
}

int CliDispatcher::cmdListBranches(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    const std::string gitBranch = cmd.get("git-branch");

    CliOutput::title("Git Branches");

    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, gitBranch);
    if (workDir.empty()) return 1;

    NeoWorkspace::GitOperations gitOps;
    auto result = gitOps.listBranches(workDir);
    if (result.exitCode != 0) {
        result = gitOps.listRemoteBranches(workDir);
        if (result.exitCode != 0) {
            CliOutput::error("Failed to list branches: " + result.stderrOutput);
            return 1;
        }
    }

    std::istringstream stream(result.stdoutOutput);
    std::string line;
    std::vector<std::string> branches;

    while (std::getline(stream, line)) {
        while (!line.empty() && (line[0] == ' ' || line[0] == '*'))
            line.erase(0, 1);
        if (line.empty()) continue;
        if (line.find("->") != std::string::npos) continue;
        if (line.find("origin/") == 0) line.erase(0, 7);
        if (line.find("remotes/") == 0) {
            auto pos = line.find('/', 8);
            if (pos != std::string::npos)
                line = line.substr(pos + 1);
        }
        branches.push_back(line);
    }

    std::sort(branches.begin(), branches.end());
    auto last = std::unique(branches.begin(), branches.end());
    branches.erase(last, branches.end());

    if (cmd.json) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& b : branches) arr.push_back(b);
        CliOutput::jsonBlock({{"category", "info"},
            {"command", "git-branches"}, {"data", {{"count", branches.size()}, {"branches", arr}}}});
        return 0;
    }

    std::vector<std::vector<std::string>> rows;
    for (const auto& b : branches) {
        rows.push_back({b});
    }

    CliOutput::table({"Branch"}, rows);
    CliOutput::info(std::to_string(branches.size()) + " branches found.");
    return 0;
}

int CliDispatcher::cmdListModpacks(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    const std::string gitBranch = cmd.get("git-branch");

    CliOutput::title("Modpack Branches");

    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    std::string workDir = ensureRepoCloned(repoUrl, gitBranch);
    if (workDir.empty()) return 1;

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found in repository root.");
        return 1;
    }

    NeoWorkspace::WorkspaceManager wm;
    if (!wm.loadFromFile(wsJson)) {
        CliOutput::error("Failed to parse workspace.json.");
        return 1;
    }

    auto branches = wm.branches();

    if (cmd.json) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& b : branches) arr.push_back(branchConfigToJson(b));
        CliOutput::jsonBlock({{"category", "info"},
            {"command", "modpacks"}, {"data", {{"count", branches.size()}, {"branches", arr}}}});
        return 0;
    }

    std::vector<std::string> headers = {"Branch", "Parent", "Game Version", "Modloader", "Hidden", "Description"};
    std::vector<std::vector<std::string>> rows;

    for (const auto& b : branches) {
        rows.push_back({
            b.name,
            b.parent.empty() ? "(root)" : b.parent,
            b.gameVersion,
            b.modloader + " " + b.modloaderVersion,
            b.hidden ? "yes" : "no",
            b.description
        });
    }

    CliOutput::table(headers, rows);
    CliOutput::info(std::to_string(branches.size()) + " modpack branches found.");
    return 0;
}

int CliDispatcher::cmdStatus(const CliCommand& cmd)
{
    const std::string repoUrl = cmd.get("repo");
    const std::string modpackBranch = cmd.get("modpack");

    CliOutput::title("Workspace Status");

    if (repoUrl.empty()) {
        CliOutput::error("No repository URL specified. Use --repo <url>.");
        return 2;
    }

    std::string workDir = resolveWorkDir(repoUrl);
    NeoWorkspace::GitOperations gitOps;

    if (!gitOps.isGitRepository(workDir)) {
        if (cmd.json) {
            CliOutput::jsonBlock({{"category", "info"}, {"command", "status"},
                {"data", {{"cloned", false}, {"workdir", workDir}, {"repo", repoUrl}}}});
            return 0;
        }
        CliOutput::info("Repository not cloned locally.");
        CliOutput::info("Run exec build --repo " + repoUrl
            + " --modpack <branch> to clone and build.");
        return 0;
    }

    std::string wsJson = findWorkspaceJson(workDir);
    if (wsJson.empty()) {
        CliOutput::error("workspace.json not found.");
        return 1;
    }

    NeoWorkspace::WorkspaceManager wm;
    if (!wm.loadFromFile(wsJson)) {
        CliOutput::error("Failed to parse workspace.json.");
        return 1;
    }

    nlohmann::json data;
    data["workdir"] = workDir;
    data["workspace"] = wm.workspaceName();
    data["minecraft_version"] = wm.minecraftVersion();
    data["modloader"] = wm.modloader();
    data["git_remote"] = wm.gitRemote();
    data["default_branch"] = wm.defaultBranch();
    data["cloned"] = true;

    auto statusResult = gitOps.status(workDir);
    if (statusResult.exitCode == 0) {
        data["git_status"] = statusResult.stdoutOutput;
    }

    auto revResult = gitOps.revParse(workDir, "HEAD");
    if (revResult.exitCode == 0) {
        data["head"] = revResult.stdoutOutput.substr(0,
            revResult.stdoutOutput.find('\n'));
    }

    auto countResult = gitOps.lsFiles(workDir);
    if (countResult.exitCode == 0) {
        std::istringstream iss(countResult.stdoutOutput);
        int fileCount = 0;
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty()) ++fileCount;
        }
        data["tracked_files"] = fileCount;
    }

    if (!modpackBranch.empty()) {
        nlohmann::json mb;
        mb["branch"] = modpackBranch;
        auto chain = wm.branchInheritanceChain(modpackBranch);
        mb["inheritance_chain"] = chain;
        auto bcfg = wm.findBranch(modpackBranch);
        if (!bcfg.name.empty()) {
            mb["game_version"] = bcfg.gameVersion;
            mb["modloader"] = bcfg.modloader;
            mb["modloader_version"] = bcfg.modloaderVersion;
        }
        std::string outputDir = findBuildOutputDir(workDir, modpackBranch);
        if (!outputDir.empty() && fs::exists(outputDir)) {
            mb["built_output"] = outputDir;
            int fileCount = 0;
            for (const auto& e : fs::recursive_directory_iterator(outputDir)) {
                if (e.is_regular_file()) ++fileCount;
            }
            mb["output_files"] = fileCount;
        }
        data["modpack"] = mb;
    }

    if (cmd.json) {
        CliOutput::jsonBlock({{"category", "info"}, {"command", "status"}, {"data", data}});
        return 0;
    }

    CliOutput::info("Workspace: " + wm.workspaceName());
    CliOutput::info("Minecraft: " + wm.minecraftVersion());
    CliOutput::info("Modloader: " + wm.modloader());
    CliOutput::info("Remote: " + wm.gitRemote());
    CliOutput::info("Default branch: " + wm.defaultBranch());

    if (data.contains("git_status")) {
        CliOutput::separator();
        CliOutput::info("Git status:");
        CliOutput::info(data["git_status"].get<std::string>());
    }

    if (data.contains("head")) {
        CliOutput::info("HEAD: " + data["head"].get<std::string>());
    }

    if (data.contains("tracked_files")) {
        CliOutput::info("Tracked files: " + std::to_string(data["tracked_files"].get<int>()));
    }

    if (!modpackBranch.empty()) {
        CliOutput::separator();
        CliOutput::info("Modpack branch: " + modpackBranch);

        auto chain = wm.branchInheritanceChain(modpackBranch);
        if (!chain.empty()) {
            std::string chainStr = chain[0];
            for (size_t i = 1; i < chain.size(); ++i)
                chainStr += " -> " + chain[i];
            CliOutput::info("Inheritance chain: " + chainStr);
        }

        auto bcfg = wm.findBranch(modpackBranch);
        if (!bcfg.name.empty()) {
            CliOutput::info("Game version: " + bcfg.gameVersion);
            CliOutput::info("Modloader: " + bcfg.modloader + " " + bcfg.modloaderVersion);
        }

        std::string outputDir = findBuildOutputDir(workDir, modpackBranch);
        if (!outputDir.empty() && fs::exists(outputDir)) {
            CliOutput::success("Built output exists: " + outputDir);
            if (data.contains("modpack") && data["modpack"].contains("output_files")) {
                CliOutput::info("Output files: "
                    + std::to_string(data["modpack"]["output_files"].get<int>()));
            }
        } else {
            CliOutput::warning("No built output found. Run build first.");
        }
    }

    return 0;
}

int CliDispatcher::cmdSyncServerConfig(const CliCommand& cmd)
{
    const std::string saveWorld = cmd.get("save");
    const std::string repoUrl = cmd.get("repo");
    const std::string gitBranch = cmd.get("git-branch");

    CliOutput::title("Sync Server Config");

    if (saveWorld.empty()) {
        CliOutput::error("No save world specified. Use --save <world_name>.");
        return 2;
    }

    fs::path savesDir;
    if (!repoUrl.empty()) {
        std::string workDir = resolveWorkDir(repoUrl);
        if (!NeoWorkspace::GitOperations().isGitRepository(workDir)) {
            CliOutput::error("Repository not cloned. Clone first.");
            return 1;
        }
        savesDir = fs::path(workDir) / ".minecraft" / "saves";
    } else {
        fs::path cwd = fs::current_path();
        for (const auto& candidate : {
            cwd / ".minecraft" / "saves",
            cwd / "saves",
            cwd.parent_path() / ".minecraft" / "saves"}) {
            if (fs::exists(candidate)) {
                savesDir = candidate;
                break;
            }
        }
        if (savesDir.empty()) {
            savesDir = cwd / "saves";
        }
    }

    fs::path worldPath = savesDir / saveWorld;
    if (!fs::exists(worldPath)) {
        CliOutput::error("Save world not found: " + worldPath.string());
        return 1;
    }

    NeoBuild::ServerConfigSync sync;
    std::string repoConfigPath;
    std::string branchName;
    if (!repoUrl.empty()) {
        repoConfigPath = resolveWorkDir(repoUrl);
        branchName = gitBranch;
        if (branchName.empty()) {
            auto br = NeoWorkspace::GitOperations().currentBranch(repoConfigPath);
            if (br.exitCode == 0) {
                branchName = br.stdoutOutput;
                branchName.erase(
                    std::remove_if(branchName.begin(), branchName.end(),
                        [](char c) { return c == '\r' || c == '\n'; }),
                    branchName.end());
            }
        }
    }

    if (!sync.init(savesDir.string(), repoConfigPath, branchName)) {
        CliOutput::error("Failed to initialize server config sync.");
        return 1;
    }

    if (!sync.hasRules()) {
        CliOutput::warning("No serverconfig rules found for branch: "
            + (branchName.empty() ? std::string("(unknown)") : branchName));
        return 0;
    }

    CliOutput::info("Scanning server configs for world: " + saveWorld);
    auto entries = sync.scanServerConfigs();

    std::vector<NeoBuild::ServerConfigEntry> worldEntries;
    for (const auto& e : entries) {
        if (e.worldName == saveWorld) {
            worldEntries.push_back(e);
        }
    }

    if (worldEntries.empty()) {
        CliOutput::info("No server config files found in world: " + saveWorld);
        return 0;
    }

    CliOutput::info("Syncing " + std::to_string(worldEntries.size()) + " config files...");

    int synced = 0;
    int failed = 0;
    for (size_t i = 0; i < worldEntries.size(); ++i) {
        if (cancelToken_.is_cancelled()) {
            CliOutput::warning("Sync cancelled by user.");
            return 0;
        }

        const auto& entry = worldEntries[i];
        int pct = static_cast<int>(((i + 1) * 100) / worldEntries.size());
        CliOutput::progress(pct, entry.relativePath);

        if (sync.syncConfig(entry, &cancelToken_)) {
            ++synced;
        } else {
            ++failed;
            CliOutput::warning("Failed to sync: " + entry.relativePath);
        }
    }

    if (failed > 0) {
        CliOutput::warning(
            "Synced " + std::to_string(synced) + " / " + std::to_string(worldEntries.size())
            + " files. " + std::to_string(failed) + " failed.");
    } else {
        CliOutput::success("All " + std::to_string(synced) + " config files synced.");
    }

    return failed > 0 ? 1 : 0;
}

} // namespace NeoCLI
