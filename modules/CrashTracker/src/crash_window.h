#pragma once

#ifdef _WIN32
#include <windows.h>
#endif

#include <QMainWindow>
#include <QTextEdit>
#include <QLabel>
#include <string>
#include <vector>
#include <map>

struct CrashInfo {
    std::string exceptionName;
    std::string exceptionCode;
    uint64_t exceptionAddress;
    std::string systemInfo;
    std::string crashingModule;
    std::string helpText;
    std::string crtMessage;
    std::map<std::string, std::string> crashDescriptions;
    std::vector<std::string> modules;
    std::vector<std::pair<uint64_t, std::string>> callStack;
};

class CrashWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit CrashWindow(QWidget* parent = nullptr);

    bool loadDump(const std::string& path);
    bool loadTraceFile(const std::string& path);
    std::string generateReport() const;
    CrashInfo info() const { return info_; }

private:
    void buildUI();
    void displayInfo();

    QLabel* titleLabel_;
    QTextEdit* reportText_;
    CrashInfo info_;
    std::string dumpPath_;

    bool parseMinidump(const std::string& path);
    const char* exceptionName(DWORD code) const;
};
