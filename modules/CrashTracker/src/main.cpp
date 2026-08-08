#include <QApplication>
#include <QMessageBox>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QCoreApplication>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#include "crash_window.h"

static std::string g_signalFile;
static QString g_watchDir;
static DWORD g_watchPid = 0;

static bool isProcessAlive(DWORD pid)
{
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    DWORD exitCode;
    bool alive = GetExitCodeProcess(h, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
#else
    return false;
#endif
}

static QString crashReportRoot()
{
    return QCoreApplication::applicationDirPath() + "/crash-report";
}

// Archive the generated report next to its dump/trace file: crash_<ts>_report.txt
static void writeReportArchive(const std::string& srcPath, const std::string& report)
{
    QFileInfo fi(QString::fromStdString(srcPath));
    if (!fi.isFile()) return;
    QString target = fi.absolutePath() + "/" + fi.completeBaseName() + "_report.txt";
    QFile f(target);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(report.c_str(), (qint64)report.size());
        f.close();
    }
}

struct ReportEntry {
    QString dmpPath;
    QString tracePath;
    QString metaPath;
    QString timestamp;
};

static QList<ReportEntry> scanReports()
{
    QList<ReportEntry> out;
    QDir root(crashReportRoot());
    auto dirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (auto& di : dirs) {
        QDir d(di.absoluteFilePath());
        QString ts = di.fileName();
        auto files = d.entryInfoList({"crash_*.dmp"}, QDir::Files);
        for (auto& fi : files) {
            ReportEntry e;
            e.dmpPath = fi.absoluteFilePath();
            QString base = fi.completeBaseName();
            QString dirPath = d.absolutePath();
            e.tracePath = dirPath + "/" + base + ".trace";
            e.metaPath = dirPath + "/" + base + ".meta";
            e.timestamp = ts;
            if (!QFileInfo::exists(e.tracePath)) e.tracePath.clear();
            if (!QFileInfo::exists(e.metaPath)) e.metaPath.clear();
            out.append(e);
        }
    }
    std::sort(out.begin(), out.end(), [](const ReportEntry& a, const ReportEntry& b) {
        return a.timestamp > b.timestamp;
    });
    return out;
}

static int cliShowEntry(const ReportEntry& e)
{
    CrashWindow* w = new CrashWindow();
    w->setAttribute(Qt::WA_DeleteOnClose);
    bool ok = false;
    if (!e.dmpPath.isEmpty())
        ok = w->loadDump(e.dmpPath.toStdString());
    else if (!e.tracePath.isEmpty())
        ok = w->loadTraceFile(e.tracePath.toStdString());

    if (!ok) {
        std::cerr << "Failed to load report: " << e.timestamp.toStdString() << std::endl;
        delete w;
        return 1;
    }
    std::string full = w->generateReport();
    {
        std::string meta = e.metaPath.toStdString();
        if (!meta.empty() && QFileInfo::exists(QString::fromStdString(meta))) {
            std::ifstream mf(meta);
            if (mf.is_open()) {
                std::string line;
                bool inCrtMsg = false;
                full += "\n--- metadata ---\n";
                while (std::getline(mf, line)) {
                    if (line == "===CRT_MESSAGE===") { inCrtMsg = true; continue; }
                    if (line == "===CRT_HELP===") { inCrtMsg = false; continue; }
                    if (line == "===CRASH_DESCRIPTIONS===") break;
                    if (inCrtMsg) {
                        full += line;
                        full += "\n";
                    }
                }
                mf.close();
            }
        }
    }
    QString src = e.dmpPath.isEmpty() ? e.tracePath : e.dmpPath;
    writeReportArchive(src.toStdString(), full);
    std::cout << full << std::endl;
    delete w;
    return 0;
}

static int cliListReports()
{
    auto reports = scanReports();
    if (reports.isEmpty()) {
        std::cout << "No crash reports found in " << crashReportRoot().toStdString() << std::endl;
        return 0;
    }
    std::cout << "Crash reports (" << reports.size() << "):\n";
    int idx = 1;
    for (auto& r : reports) {
        std::cout << "  " << idx++ << ". " << r.timestamp.toStdString()
                  << "  " << r.dmpPath.toStdString() << std::endl;
    }
    return 0;
}

static void checkSignal()
{
    // 1. Check signal file
    if (!g_signalFile.empty()) {
        QFile f(QString::fromStdString(g_signalFile));
        if (f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString path = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            f.remove();
            if (!path.isEmpty() && QFileInfo::exists(path)) {
                static bool shown = false;
                if (!shown) {
                    shown = true;
                    auto* w = qobject_cast<CrashWindow*>(QApplication::activeWindow());
                    if (!w) {
                        w = new CrashWindow();
                        w->setAttribute(Qt::WA_DeleteOnClose);
                    }
                    w->loadDump(path.toStdString());
                    w->show();
                }
            }
        }
    }

    // 2. Check if watched process died — scan for recent dumps
    if (g_watchPid && !isProcessAlive(g_watchPid)) {
        QDir dir(g_watchDir);
        auto files = dir.entryInfoList({"crash_*.dmp"}, QDir::Files, QDir::Time);
        for (auto& fi : files) {
            if (fi.lastModified().secsTo(QDateTime::currentDateTime()) < 120) {
                static bool shown = false;
                if (!shown) {
                    shown = true;
                    auto* w = qobject_cast<CrashWindow*>(QApplication::activeWindow());
                    if (!w) {
                        w = new CrashWindow();
                        w->setAttribute(Qt::WA_DeleteOnClose);
                    }
                    w->loadDump(fi.absoluteFilePath().toStdString());
                    w->show();
                }
                break;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    bool cliMode = false;
    bool cliList = false;
    bool cliLatest = false;
    bool hasDumpArg = false;
    std::string dumpPath;

    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);
        if (a == "--cli") {
            cliMode = true;
        } else if (a == "--list") {
            cliMode = true;
            cliList = true;
        } else if (a == "--latest") {
            cliMode = true;
            cliLatest = true;
        } else if (a.size() > 4 && (a.rfind(".dmp") == a.size() - 4
                || a.rfind(".trace") == a.size() - 6
                || a.rfind(".txt") == a.size() - 4)) {
            dumpPath = a;
            hasDumpArg = true;
        }
    }

    if (cliMode) {
#ifdef _WIN32
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == nullptr || hOut == INVALID_HANDLE_VALUE) {
            if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
                AllocConsole();
            }
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
        }
#endif
        QApplication app(argc, argv);
        app.setApplicationName("CrashTracker");
        app.setOrganizationName("HiBer2007");

        if (cliList) {
            return cliListReports();
        }

        if (hasDumpArg && !dumpPath.empty()) {
            CrashWindow w;
            bool ok = false;
            if (dumpPath.find(".dmp") != std::string::npos)
                ok = w.loadDump(dumpPath);
            else
                ok = w.loadTraceFile(dumpPath);
            if (!ok) return 1;
            std::string report = w.generateReport();
            writeReportArchive(dumpPath, report);
            std::cout << report << std::endl;
            return 0;
        }

        if (cliLatest) {
            auto reports = scanReports();
            if (reports.isEmpty()) {
                std::cout << "No crash reports found in "
                          << crashReportRoot().toStdString() << std::endl;
                return 0;
            }
            return cliShowEntry(reports.first());
        }

        std::cerr << "Usage: CrashTracker.exe --cli <file.dmp|file.trace>\n"
                  << "       CrashTracker.exe --cli --list\n"
                  << "       CrashTracker.exe --cli --latest\n";
        return 1;
    }

    QApplication app(argc, argv);
    app.setApplicationName("CrashTracker");
    app.setOrganizationName("HiBer2007");

    // Parse args: --watch=<signalFile> --watch-pid=<pid> --watch-dir=<dir>
    QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        QString a = args[i];
        if (a.startsWith("--watch=")) {
            g_signalFile = a.mid(8).toStdString();
        } else if (a.startsWith("--watch-pid=")) {
            g_watchPid = a.mid(12).toUInt();
        } else if (a.startsWith("--watch-dir=")) {
            g_watchDir = a.mid(12);
        } else if (a.endsWith(".dmp", Qt::CaseInsensitive)) {
            // Direct dump file — open immediately
            CrashWindow* w = new CrashWindow();
            w->setAttribute(Qt::WA_DeleteOnClose);
            w->loadDump(a.toStdString());
            w->show();
            return app.exec();
        }
    }

    // Watch mode: background polling
    if (!g_signalFile.empty() || g_watchPid) {
        auto* timer = new QTimer(&app);
        QObject::connect(timer, &QTimer::timeout, &checkSignal);
        timer->start(500);
        return app.exec();
    }

    // Standalone mode: show window with open dialog
    CrashWindow window;
    window.show();
    return app.exec();
}
