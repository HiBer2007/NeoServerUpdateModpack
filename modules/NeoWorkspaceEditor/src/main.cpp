#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <iostream>
#include <crtdbg.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "editor_window.h"

#include <logger.h>
#include <crash_reporter.h>
#include <git_operations.h>
#include <install_config.h>

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
// 控制台子系统 EXE 的终端持有策略 (与主程序 holdOrReleaseConsole 同款):
//  - 从终端启动: 已继承父控制台 (cmd 共享) -> GetConsoleProcessList >1 -> 保持连接,
//    并设 UTF-8 代码页 + VT (否则 spdlog UTF-8 日志按 GBK 乱码)
//  - 从 Explorer 双击启动: OS 新建独占控制台 -> 进程数 ==1 -> FreeConsole 释放
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

int main(int argc, char* argv[])
{
#ifdef _WIN32
    holdOrReleaseConsole();
#endif

    auto* app = new QApplication(argc, argv);
    app->setApplicationName("NSUMEditor");
    app->setOrganizationName("HiBer2007");
    app->setApplicationDisplayName("NSUM 仓库管理器");
    app->setApplicationVersion("1.0.0");

    HiBerCTM::InstallCrashHandler();
    HiBerCTM::InstallCrtReportHook();
    HiBerCTM::SetCrashAppName("NSUM Editor");
    HiBerCTM::SetCrashHelpText(
        "  1. Verify the repository directory is a valid Git repository.\n"
        "  2. Check that workspace.json exists and is valid JSON.\n"
        "  3. If cloning fails, verify SSH keys (Repository -> SSH Key Manager).\n"
        "  4. For HTTPS clone failures, check credentials or use SSH instead.\n"
        "  5. Git operations require git.exe in PATH or configured in install.conf.\n"
        "  6. The editor saves settings to config/custom/editor.ini — delete to reset.\n"
        "  7. Report bugs with the full crash-report/<timestamp>/ contents attached.");
    HiBerCTM::AddCrashTypeInfo("ACCESS_VIOLATION",
        "A null or dangling pointer was dereferenced, likely in the editor UI.\n"
        "This often occurs when a UI element was not properly initialized.\n"
        "Check the call stack for the specific widget or action involved.");
    HiBerCTM::AddCrashTypeInfo("STACK_OVERFLOW",
        "Stack overflow. Check for circular branch inheritance in the branch\n"
        "editor that may cause infinite recursion when building the tree view.");
    HiBerCTM::AddCrashTypeInfo("CPP_EXCEPTION",
        "Unhandled C++ exception. Check the call stack.\n"
        "Common causes: malformed JSON in workspace.json, Git operation failure,\n"
        "or missing plugin DLLs in the deploy directory.");
    CLogger::Init("workspace_editor.log", "editor");
#ifdef _WIN32
    std::cout << "[EDITOR] Log output to this terminal" << std::endl;
#endif
    CLogger::Info("=== Editor Mode Started ===");
    CLogger::Info("Working directory: {}", QCoreApplication::applicationDirPath().toStdString());

    // Git 环境: 与主程序同款加载 install.conf / tools/git, 统一领域层与 GitPanel 的 git 路径
    auto icfg = InstallConfig::load();
    if (!icfg.valid) {
        std::cerr << icfg.errorMsg << std::endl;
        CLogger::Error("{}", icfg.errorMsg);
    }
    NeoWorkspace::GitOperations::SetDefaultGitPath(icfg.gitPath);
    CLogger::Info("Git: {} ({})", icfg.gitPath, icfg.useSystemGit ? "system" : "bundled");

    QCommandLineParser parser;
    parser.setApplicationDescription("NeoServer 工作区配置文件编辑器");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "要打开的 workspace.json 文件路径", "[file]");

    parser.process(*app);

    auto* window = new EditorWindow();
    window->show();
    heapCheckPoint("after show");

    QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        QString path = args.first();
        QFileInfo fi(path);
        if (fi.isDir())
            window->loadWorkspace(fi.absoluteFilePath().toStdString());
        else
            window->loadWorkspace(fi.absolutePath().toStdString());
    }

    int result = app->exec();
    CLogger::Info("Editor exited, exec returned");
    heapCheckPoint("after exec");

    delete window;
    heapCheckPoint("after delete window");

    delete app;
    heapCheckPoint("after delete app");
    return result;
}



