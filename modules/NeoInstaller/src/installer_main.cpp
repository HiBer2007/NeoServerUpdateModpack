#include <QApplication>
#include <QCoreApplication>
#include <iostream>
#include <string>
#include <set>

#ifdef _WIN32
#include <windows.h>
#endif

#include "installer_wizard.h"
#include "silent_installer.h"

static void printHelp()
{
    std::cout << "NeoInstaller — NeoServer Setup\n\n"
              << "Usage:\n"
              << "  NeoInstaller.exe [options]\n"
              << "  NeoInstaller.exe --silent --path <path> [options]\n\n"
              << "Options:\n"
              << "  --silent, -s        Silent install mode (no GUI, terminal output)\n"
              << "  --path <path>       Install target directory\n"
              << "  --with-editor       Install Workspace Editor component\n"
              << "  --use-system-git    Prefer system-installed Git\n"
              << "  --use-bundled-git   Force install bundled Git (ignore system Git)\n"
              << "  --help, -h          Show this help\n\n"
              << "Git behavior:\n"
              << "  Default (no flags): Auto-detect — use system Git if found, else install bundled.\n"
              << "  Git is always installed. With --with-editor: MinGit; without: MinGit.\n\n"
              << "Examples:\n"
              << "  NeoInstaller.exe\n"
              << "  NeoInstaller.exe --with-editor\n"
              << "  NeoInstaller.exe --silent --path \"D:\\NeoServer\" --with-editor\n";
}

int main(int argc, char* argv[])
{
    bool silent = false;
    bool withEditor = false;
    std::string installPath;
    int gitMode = 0; // 0=Auto, 1=UseSystem, 2=UseBundled
    std::set<std::string> knownArgs = {
        "--help", "-h", "--silent", "-s", "--path", "--with-editor",
        "--use-system-git", "--use-bundled-git"
    };

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--help" || arg == "-h") {
#ifdef _WIN32
            AttachConsole(ATTACH_PARENT_PROCESS);
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
#endif
            printHelp();
            return 0;
        }

        if (arg == "--silent" || arg == "-s") {
            silent = true;
            continue;
        }
        if (arg == "--path" && i + 1 < argc) {
            installPath = argv[++i];
            continue;
        }
        if (arg == "--with-editor") {
            withEditor = true;
            continue;
        }
        if (arg == "--use-system-git") {
            gitMode = 1;
            continue;
        }
        if (arg == "--use-bundled-git") {
            gitMode = 2;
            continue;
        }

        if (knownArgs.count(arg)) continue;

        std::cerr << "Error: unknown option '" << arg << "'\n\n";
        printHelp();
        return 1;
    }

    if (silent) {
#ifdef _WIN32
        if (!AttachConsole(ATTACH_PARENT_PROCESS))
            AllocConsole();
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
#endif
        QCoreApplication app(argc, argv);
        app.setApplicationName("NeoServer Installer");
        app.setOrganizationName("HiBer2007");

        NeoInstaller::SilentInstaller installer;
        installer.setInstallEditor(withEditor);
        if (gitMode == 1) installer.setGitMode(NeoInstaller::SilentInstaller::UseSystem);
        if (gitMode == 2) installer.setGitMode(NeoInstaller::SilentInstaller::UseBundled);
        std::cout << "NeoInstaller Silent Mode" << std::endl;
        std::cout << "Install path: " << (installPath.empty() ? "(default)" : installPath) << std::endl;
        std::cout << "Editor: " << (withEditor ? "yes" : "no") << std::endl;
        std::cout << "Git mode: " << (gitMode == 1 ? "system" : (gitMode == 2 ? "bundled" : "auto")) << std::endl;
        return installer.run(installPath) ? 0 : 1;
    }

#ifdef _WIN32
    FreeConsole();
#endif
    QApplication app(argc, argv);
    app.setApplicationName("NeoServer Installer");
    app.setOrganizationName("HiBer2007");

    NeoInstaller::InstallerWizard wizard;
    wizard.setInstallEditor(withEditor);
    wizard.show();
    return app.exec();
}
