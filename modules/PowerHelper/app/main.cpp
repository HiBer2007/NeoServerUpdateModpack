#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#endif

#include <crash_reporter.h>
#include <logger.h>

#include "reader_window.h"
#include "powerhelper_cli.h"

static void printUsage(std::ostream& os)
{
    os << "PowerHelper - Markdown 文档阅读器\n\n";
    os << "用法:\n";
    os << "  PowerHelper <file.md>            单文档模式\n";
    os << "  PowerHelper <dir>                文档组模式 (含目录显示)\n";
    os << "  PowerHelper render <file.md>     CLI: 单文档终端渲染\n";
    os << "  PowerHelper toc <file.md>        CLI: 打印标题 TOC\n";
    os << "  PowerHelper group <dir> [--toc]  CLI: 文档组文件列表 / TOC 映射\n";
    os << "  -h/--help/-help/help//h//?/-?    帮助\n";
    os << "  -v/--version//v                  版本\n";
    os << "  --json                           CLI: JSON 标记块输出\n";
}

static int runCliMode(int argc, char* argv[])
{
#ifdef _WIN32
    // PowerHelper 为控制台子系统 EXE (与主程序同款): CLI stdout 原生连接控制台。
    // renderToTerminal 输出 UTF-8 + ANSI 转义序列, 需:
    //   ① 控制台代码页设为 UTF-8 (否则中文/表格线按 GBK 解码成乱码)
    //   ② 启用 VT 处理 (ENABLE_VIRTUAL_TERMINAL_PROCESSING, 否则 [1m/[0m 等按字面显示)
    //   ③ stdout/stderr 置二进制模式 (_O_BINARY), 否则 CRT 按 ANSI 代码页做宽字符转换,
    //      UTF-8 中文会被替换成 '?' (U+FFFD); 原样透传字节由 UTF-8 控制台解析
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != nullptr && hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode))
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
    // CLI 崩溃报告: 拉起 CrashTracker --cli 控制台输出, 不弹 GUI
    // (SetCrashCliMode 已在 main 内 InstallCrashHandler 之前设置, 此处无需重复)
    QCoreApplication app(argc, argv);
    return PowerHelper::runCli(argc, argv);
}

int main(int argc, char* argv[])
{
    const bool cliRequested = argc > 1
        && (std::string(argv[1]) == "render"
            || std::string(argv[1]) == "toc"
            || std::string(argv[1]) == "group");
    if (cliRequested)
        HiBerCTM::SetCrashCliMode(true);

    HiBerCTM::InstallCrashHandler();
    HiBerCTM::InstallCrtReportHook();
    HiBerCTM::SetCrashAppName("PowerHelperHandler");
    HiBerCTM::SetCrashHelpText(
        "  1. If using CLI mode (render/toc/group), verify the file or directory argument exists.\n"
        "  2. Check that the WebView2 Runtime is installed; without it, rendering falls back to Qt.\n"
        "  3. Verify PowerHelperCore.dll and its Qt6 dependencies are present next to PowerHelper.exe.\n"
        "  4. If the crash involves missing fonts or locale, ensure the system code page is UTF-8.\n"
        "  5. Run CrashTracker.exe --cli --latest to re-print this report in the same terminal.\n"
        "  6. Report bugs with the full crash-report/<timestamp>/ contents attached.");

    if (argc > 1) {
        const std::string a1 = argv[1];
        if (a1 == "-h" || a1 == "--help" || a1 == "-help" || a1 == "help"
            || a1 == "/h" || a1 == "/?" || a1 == "-?") {
            printUsage(std::cout);
            return 0;
        }
        if (a1 == "-v" || a1 == "--version" || a1 == "/v") {
            std::cout << "PowerHelper v1.0.0" << std::endl;
            return 0;
        }
        if (a1 == "render" || a1 == "toc" || a1 == "group") {
            return runCliMode(argc, argv);
        }
    }

#ifdef _WIN32
    // 控制台子系统 EXE: GUI 模式若从资源管理器双击启动, Windows 会为控制台子系统
    // 进程自带一个控制台窗口; GetConsoleProcessList 返回 1 (独占控制台=双击) 时
    // FreeConsole 释放该自带窗口。从已打开的终端启动时控制台被共享 (进程数>1),
    // 保持连接 (与主程序 holdOrReleaseConsole 同款)。
    DWORD consolePids[8];
    const DWORD consoleCount = GetConsoleProcessList(consolePids, 8);
    if (consoleCount <= 1)
        FreeConsole();
#endif

    QApplication app(argc, argv);
    app.setApplicationName("PowerHelper");
    app.setOrganizationName("HiBer2007");
    app.setApplicationDisplayName("PowerHelper 文档阅读器");

    CLogger::Init("powerhelper.log", "powerhelper");

    QString target;
    QString anchor;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--anchor" && i + 1 < argc) {
            anchor = QString::fromUtf8(argv[++i]);
            continue;
        }
        if (target.isEmpty() && !a.empty() && a[0] != '-')
            target = QString::fromUtf8(argv[i]);
    }
    if (!target.isEmpty()) {
        const QFileInfo fi(target);
        if (!fi.exists()) {
            std::cerr << "File or directory not found: "
                      << target.toStdString() << std::endl;
            printUsage(std::cerr);
            return 2;
        }
    }

    auto* window = new PowerHelper::ReaderWindow();
    window->setAttribute(Qt::WA_DeleteOnClose);

    if (!target.isEmpty()) {
        const QFileInfo fi(target);
        if (fi.isDir())
            window->openGroup(fi.absoluteFilePath());
        else if (fi.isFile()) {
            window->openFile(fi.absoluteFilePath());
            window->scrollToHeadingText(anchor);
        }
    } else {
        const QString docs = QCoreApplication::applicationDirPath()
            + QStringLiteral("/docs");
        if (QDir(docs).exists())
            window->openGroup(docs);
    }
    window->show();
    return app.exec();
}

