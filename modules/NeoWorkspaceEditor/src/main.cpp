#include <QApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QTimer>
#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <crtdbg.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "editor_window.h"
#include "editor_tui.h"

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
//  - 从 Explorer 双击启动: OS 新建独占控制台 -> 进程数 ==1 -> 不提前释放,
//    延迟到主窗口真正 show 之后 (main 中 QTimer::singleShot(0, FreeConsole))
static bool shouldKeepConsole()
{
    DWORD pids[8];
    const DWORD n = GetConsoleProcessList(pids, 8);
    return n > 1;
}

static void setupConsoleUtf8()
{
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
    // TUI 需要 UTF-8 代码页 + VT, 两种启动方式都设置 (双击的独占控制台随后释放)
    setupConsoleUtf8();
#endif

#ifdef _WIN32
    const bool keepConsole = shouldKeepConsole();
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
    // 窗口加载期间渲染叠层 TUI (居中拼接字卡片 + 顶部日志滚动 + 底部进度条):
    // 两种启动方式都渲染 —— 终端启动加载完后 stop() 重放缓冲日志并恢复正常输出;
    // 双击启动加载完后 stop() 再释放独占控制台 (黑窗加载动画即 OS 新建的终端)
    // 仅 stdout 连接真实控制台时渲染 (cmd 重定向到文件时直接输出日志, 不渲染)
    // 必须在第一条 CLogger 日志之前 start(): 否则 TUI 前的日志直接走 stdout
    // 会被首帧清屏抹掉且不在缓冲中, 停止后重放丢失 (2026-08-09 用户实测窗口加载日志缺失)
    std::unique_ptr<nsum_tui::EditorTui> tui;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool stdoutIsConsole = (hOut != nullptr && hOut != INVALID_HANDLE_VALUE
        && GetFileType(hOut) == FILE_TYPE_CHAR);
    if (stdoutIsConsole) {
        tui = std::make_unique<nsum_tui::EditorTui>();
        tui->setStatus("Loading editor UI...");
        tui->start();
    }
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

#ifdef _WIN32
    if (tui) {
        // 窗口创建前的停留: TUI 加载动画展示期 (进度条 0→90 按时间推进),
        // 窗口弹出即 TUI 结束。NSUM_TUI_HOLD_MS=0 关闭 (2026-08-09 用户要求:
        // 等待应插入于窗口创建之前, 而非窗口弹出之后)
        int holdMs = 1200;
        if (const char* env = std::getenv("NSUM_TUI_HOLD_MS")) {
            holdMs = std::atoi(env);
        }
        if (holdMs > 0) {
            const int steps = (std::max)(1, holdMs / 100);
            for (int i = 1; i <= steps; ++i) {
                tui->setProgress(90 * i / steps);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
#endif

    CLogger::Info("EditorMain: creating EditorWindow ...");
    if (tui) {
        tui->setProgress(95);   // hold 已达 90, 保持单调递增
    }
    auto* window = new EditorWindow();
    CLogger::Info("EditorMain: EditorWindow created");
    if (tui) {
        tui->setProgress(98);
    }
    window->show();
    CLogger::Info("EditorMain: window shown");

#ifdef _WIN32
    if (tui) {
        // 窗口展示后: 进度条推到 100% 并立即结束 TUI (不再停留)
        tui->setProgress(100);
        tui->setStatus("Editor window ready");
        tui->stop();
    }
    heapCheckPoint("after show");
    if (keepConsole) {
        std::cout << "[EDITOR] Log output to this terminal" << std::endl;
    } else {
        // 双击启动 (OS 新建独占控制台): TUI 已渲染加载动画, 主窗口展示后释放
        QTimer::singleShot(0, []() {
            FreeConsole();
        });
    }
#endif

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



