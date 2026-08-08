#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <iostream>
#include <string>
#include <memory>
#include <filesystem>
#include <vector>

#include <cancel_token.h>
#include <logger.h>
#include <crash_reporter.h>

#include <arg_parser.h>
#include <cli_dispatcher.h>
#include <cli_output.h>
#include <git_operations.h>

#include <wizard_window.h>

#include "splash_window.h"
#include "install_config.h"

#include <crtdbg.h>

static void heapCheckPoint(const char* where)
{
    int bad = _CrtCheckMemory() ? 0 : 1;
    if (bad)
        CLogger::Error("HEAP CHECK FAILED at: {}", where);
    else
        CLogger::Info("HEAP CHECK OK at: {}", where);
    std::cerr << "[HEAP-CHECK] " << where << " -> " << (bad ? "CORRUPT" : "OK") << std::endl;
}

#ifdef _WIN32
#include <windows.h>
static NeoCore::CancelToken* g_cancelToken = nullptr;
static BOOL WINAPI consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT && g_cancelToken) {
        g_cancelToken->request_cancel();
        std::cerr << "\n[CANCELLED] Interrupted by user\n";
        return TRUE;
    }
    return FALSE;
}

// 控制台子系统 EXE 的终端持有策略 (与 GUI 模式同用):
//  - 从终端启动: 进程启动时已继承父控制台 (cmd/pwsh 共享同一控制台), GetConsoleProcessList
//    返回 >1 -> 保持连接, 日志继续输出到该终端; 并设 UTF-8 代码页 + VT, 否则 spdlog 输出的
//    UTF-8 中文日志按 GBK 解码成乱码 (2026-08-07 用户实测 Wizard鏋勯€? 等)
//  - 从 Explorer 双击启动: OS 为本进程新建独占控制台, GetConsoleProcessList 返回 1
//    -> FreeConsole 释放该自带窗口
// 注意: 不能用 "AttachConsole(ATTACH_PARENT_PROCESS) 失败即 FreeConsole" —— 已继承控制台时
// AttachConsole 恒失败 (ERROR_ACCESS_DENIED), 会把终端误断开 (2026-08-07 用户实测无日志)。
static void holdOrReleaseConsole()
{
    DWORD pids[8];
    const DWORD n = GetConsoleProcessList(pids, 8);
    if (n <= 1) {
        FreeConsole();
        return;
    }
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != nullptr && hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
}
#endif

namespace fs = std::filesystem;

static int runFlowGuiMode(int argc, char* argv[], const NeoCLI::CliCommand& cmd);

static void printBanner(std::ostream& os = std::cout)
{
    os << "    _   _______ __  ____  ___\n";
    os << "   / | / / ___// / / /  |/  /\n";
    os << "  /  |/ /\\__ \\/ / / / /|_/ /\n";
    os << " / /|  /___/ / /_/ / /  / /\n";
    os << "/_/ |_//____/\\____/_/  /_/ \n";
    os << "NeoServerUpdateModpack v1.0.0\n\n";
}

static bool verifyRuntimeEnvironment(bool quiet = false)
{
    std::vector<std::string> missing;

    auto checkDir = [&](const std::string& dir, const std::string& desc) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) {
            missing.push_back("[" + desc + "] Directory not found: " + dir);
        }
    };

    auto checkFile = [&](const std::string& path, const std::string& desc) {
        if (!fs::exists(path)) {
            missing.push_back("[" + desc + "] File not found: " + path);
        }
    };

    std::string exeDir = QCoreApplication::applicationDirPath().toStdString();
    checkDir(exeDir + "/parsers", "Config Parsers");
    checkDir(exeDir + "/pointers", "Pointer Resolvers");
    checkDir(exeDir + "/exporters", "Modpack Exporters");

    for (auto& m : missing) {
        CLogger::Warn("{}", m);
        if (!quiet) {
            std::cerr << "[WARN] " << m << std::endl;
        }
    }

    return missing.empty();
}

static int runCliMode(int argc, char* argv[])
{
    bool jsonMode = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--json") {
            jsonMode = true;
            break;
        }
    }
    std::ostream& cliOut = jsonMode ? std::cerr : std::cout;

    NeoCLI::ArgParser parser;
    NeoCLI::CliCommand cmd = parser.parse(argc, argv);

    if (!cmd.error.empty()) {
        std::cerr << "Unknown command: '" << cmd.error << "'. Use --help for usage." << std::endl;
        return 2;
    }

    if (cmd.category == NeoCLI::CliCategory::Version) {
        parser.printVersion();
        return 0;
    }
    if (cmd.category == NeoCLI::CliCategory::Help
        || (cmd.help && cmd.category == NeoCLI::CliCategory::None)) {
        parser.printHelp();
        return 0;
    }
    if (cmd.help) {
        parser.printHelp(cmd.category);
        return 0;
    }

    if (cmd.category == NeoCLI::CliCategory::Flow && cmd.verb == "gui") {
        return runFlowGuiMode(argc, argv, cmd);
    }

    QCoreApplication app(argc, argv);
    HiBerCTM::SetCrashCliMode(true);
    CLogger::Init("builder.log", "builder");
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("NeoServerUpdateModpackHandler");
    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode, verify --repo and --modpack parameters are correct.\n"
        "  2. Check that the Git repository is accessible and workspace.json is valid.\n"
        "  3. Ensure all parser/pointer/exporter plugins are present in deploy/ directory.\n"
        "  4. Check network connectivity for remote repositories and pointer downloads.\n"
        "  5. If the crash involves Qt6*.dll, run windeployqt to redeploy Qt plugins.\n"
        "  6. Verify .minecraft/versions/.cache/ has sufficient disk space.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");

    auto cfg = InstallConfig::load();
    if (!cfg.valid) {
        std::cerr << cfg.errorMsg << std::endl;
        CLogger::Error("{}", cfg.errorMsg);
        return 1;
    }

    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode, verify --repo and --modpack parameters are correct.\n"
        "  2. Check that the Git repository is accessible and workspace.json is valid.\n"
        "  3. Ensure all parser/pointer/exporter plugins are present in deploy/ directory.\n"
        "  4. Check network connectivity for remote repositories and pointer downloads.\n"
        "  5. If the crash involves Qt6*.dll, run windeployqt to redeploy Qt plugins.\n"
        "  6. Verify .minecraft/versions/.cache/ has sufficient disk space.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");
    HiBerCTM::AddCrashTypeInfo("ACCESS_VIOLATION",
        "A null or dangling pointer was dereferenced.\n"
        "Check that parsers/, pointers/, exporters/ directories contain all\n"
        "expected .dll and .meta.json files.");
    HiBerCTM::AddCrashTypeInfo("STACK_OVERFLOW",
        "Stack overflow — check for circular dependencies in branch inheritance\n"
        "chains or sync rule evaluation causing unbounded recursion.");
    HiBerCTM::AddCrashTypeInfo("CPP_EXCEPTION",
        "Unhandled C++ exception. Check the call stack.\n"
        "Common causes: invalid JSON, network timeout, missing plugin dependencies.");
    NeoWorkspace::GitOperations::SetDefaultGitPath(cfg.gitPath);
    cliOut << "[INIT] Git: " << cfg.gitPath << " (" << (cfg.useSystemGit ? "system" : "bundled") << ")" << std::endl;

    std::string exeDir = QCoreApplication::applicationDirPath().toStdString();
    cliOut << "[INIT] Working directory: " << exeDir << std::endl;

    verifyRuntimeEnvironment(false);

    if (!cmd.silent) {
        printBanner(jsonMode ? std::cerr : std::cout);
    }

#ifdef _WIN32
    NeoCore::CancelToken cancelToken;
    g_cancelToken = &cancelToken;
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#endif

    NeoCLI::CliDispatcher dispatcher;
    dispatcher.setGitConfig(cfg.gitPath, cfg.useSystemGit);
    cliOut << "[EXEC] Starting " << NeoCLI::ArgParser::categoryName(cmd.category)
           << " " << cmd.verb << "..." << std::endl;
    int result = dispatcher.dispatch(cmd);

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, FALSE);
    g_cancelToken = nullptr;
#endif

    return result;
}

static int runFlowGuiMode(int argc, char* argv[], const NeoCLI::CliCommand& cmd)
{
    GUIWorker::FlowConfig fcfg;
    if (cmd.has("from")) {
        fcfg.startPage = QString::fromStdString(cmd.get("from"));
        if (GUIWorker::WizardWindow::pageNameToIndex(fcfg.startPage) < 0) {
            std::cerr << "Invalid --from page: '" << fcfg.startPage.toStdString()
                      << "'. Use --help for usage." << std::endl;
            return 2;
        }
    }
    if (cmd.has("to")) {
        fcfg.endPage = QString::fromStdString(cmd.get("to"));
        if (GUIWorker::WizardWindow::pageNameToIndex(fcfg.endPage) < 0) {
            std::cerr << "Invalid --to page: '" << fcfg.endPage.toStdString()
                      << "'. Use --help for usage." << std::endl;
            return 2;
        }
    }
    fcfg.collectOnly = cmd.has("collect-only");
    for (const auto& kv : cmd.prefill) {
        auto eq = kv.find('=');
        if (eq == std::string::npos) {
            std::cerr << "Invalid --prefill '" << kv << "'. Expected key=value. Use --help for usage." << std::endl;
            return 2;
        }
        QString k = QString::fromStdString(kv.substr(0, eq)).trimmed();
        QString v = QString::fromStdString(kv.substr(eq + 1));
        if (k.isEmpty() || v.isEmpty()) {
            std::cerr << "Invalid --prefill '" << kv << "'. Expected key=value. Use --help for usage." << std::endl;
            return 2;
        }
        fcfg.prefill[k] = v;
    }

#ifdef _WIN32
    holdOrReleaseConsole();
#endif

    QApplication app(argc, argv);
    app.setApplicationName("NeoServerUpdateModpack");
    app.setOrganizationName("HiBer2007");
    app.setApplicationDisplayName("NSUM构建工具");

    CLogger::Init("builder.log", "builder");
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("NeoServerUpdateModpackHandler");
    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode, verify --repo and --modpack parameters are correct.\n"
        "  2. Check that the Git repository is accessible and workspace.json is valid.\n"
        "  3. Ensure all parser/pointer/exporter plugins are present in deploy/ directory.\n"
        "  4. Check network connectivity for remote repositories and pointer downloads.\n"
        "  5. If the crash involves Qt6*.dll, run windeployqt to redeploy Qt plugins.\n"
        "  6. Verify .minecraft/versions/.cache/ has sufficient disk space.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");

    auto icfg = InstallConfig::load();
    if (!icfg.valid) {
        std::cerr << icfg.errorMsg << std::endl;
        CLogger::Error("{}", icfg.errorMsg);
        return 1;
    }
    NeoWorkspace::GitOperations::SetDefaultGitPath(icfg.gitPath);

    auto* wizard = new GUIWorker::WizardWindow();
    wizard->setAttribute(Qt::WA_DeleteOnClose);

    int exitCode = 0;
    bool completed = false;
    QObject::connect(wizard, &GUIWorker::WizardWindow::flowDataReady,
        [&exitCode, &completed](const QString& json) {
            std::cout << "=====JSON-BEGIN=====\n" << json.toStdString()
                      << "\n=====JSON-END=====" << std::endl;
            exitCode = 0;
            completed = true;
        });
    QObject::connect(wizard, &QObject::destroyed, [&exitCode, &completed, &app]() {
        if (!completed) {
            std::cerr << "Flow cancelled." << std::endl;
            exitCode = 1;
        }
        app.exit(exitCode);
    });

    wizard->setFlowMode(fcfg);

    if (wizard->flowDone()) {
        wizard->deleteLater();
    } else {
        wizard->show();
        if (qEnvironmentVariableIsSet("NSUM_FLOW_AUTOFINISH")) {
            QTimer::singleShot(1500, wizard, [wizard]() {
                wizard->flowTriggerNext();
            });
        }
    }

    app.exec();
    return exitCode;
}

static int runGuiMode(int argc, char* argv[])
{
#ifdef _WIN32
    holdOrReleaseConsole();
#endif
    QApplication* app = new QApplication(argc, argv);
    app->setApplicationName("NeoServerUpdateModpack");
    app->setOrganizationName("HiBer2007");
    app->setApplicationDisplayName("NSUM构建工具");

    std::string exeDir = QCoreApplication::applicationDirPath().toStdString();

    CLogger::Init("gui.log", "gui");
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::SetCrashAppName("NeoServerUpdateModpackHandler");
    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode, verify --repo and --modpack parameters are correct.\n"
        "  2. Check that the Git repository is accessible and workspace.json is valid.\n"
        "  3. Ensure all parser/pointer/exporter plugins are present in deploy/ directory.\n"
        "  4. Check network connectivity for remote repositories and pointer downloads.\n"
        "  5. If the crash involves Qt6*.dll, run windeployqt to redeploy Qt plugins.\n"
        "  6. Verify .minecraft/versions/.cache/ has sufficient disk space.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");

    auto cfg = InstallConfig::load();
    if (!cfg.valid) {
        CLogger::Error("{}", cfg.errorMsg);
        QMessageBox::critical(nullptr, "Fatal Error",
            QString::fromStdString(cfg.errorMsg));
        return 1;
    }
    NeoWorkspace::GitOperations::SetDefaultGitPath(cfg.gitPath);
    CLogger::Info("Git: {} ({})", cfg.gitPath, cfg.useSystemGit ? "system" : "bundled");

    CLogger::Info("=== GUI Mode Started ===");

    CLogger::Info("Working directory: {}", exeDir);

    SplashWindow splash("NSUM构建工具");
    splash.setStatus("正在初始化核心组件...");
    splash.show();
    QApplication::processEvents();
    heapCheckPoint("after splash show");

    splash.setStatus("正在检查运行环境...");
    bool envOk = verifyRuntimeEnvironment(true);
    heapCheckPoint("after verify env");
    if (!envOk) {
        CLogger::Warn("Runtime environment incomplete - some features may be unavailable");
    }

    QTimer::singleShot(200, [&splash, app]() {
        splash.setStatus("正在加载用户界面...");
        QApplication::processEvents();

        QTimer::singleShot(100, [&splash]() {
            splash.setProgress(100, 100);
            QApplication::processEvents();

            auto* wizard = new GUIWorker::WizardWindow();
            wizard->setAttribute(Qt::WA_DeleteOnClose);
            wizard->show();
            heapCheckPoint("after wizard show");
            splash.close();
        });
    });

    int result = app->exec();
    CLogger::Info("GUI exited, exec returned");
    heapCheckPoint("after exec");

    delete app;
    heapCheckPoint("after delete app");
    return result;
}

int main(int argc, char* argv[])
{
    HiBerCTM::InstallCrashHandler();
    HiBerCTM::InstallCrtReportHook();
    HiBerCTM::SetCrashAppName("NeoServerUpdateModpackHandler");
    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode, verify --repo and --modpack parameters are correct.\n"
        "  2. Check that the Git repository is accessible and workspace.json is valid.\n"
        "  3. Ensure all parser/pointer/exporter plugins are present in deploy/ directory.\n"
        "  4. Check network connectivity for remote repositories and pointer downloads.\n"
        "  5. If the crash involves Qt6*.dll, run windeployqt to redeploy Qt plugins.\n"
        "  6. Verify .minecraft/versions/.cache/ has sufficient disk space.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");

    if (argc > 1) {
        return runCliMode(argc, argv);
    }

    return runGuiMode(argc, argv);
}

#include "main.moc"

