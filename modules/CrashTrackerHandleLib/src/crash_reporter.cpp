#include "crash_reporter.h"

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#include <shellapi.h>
#include <crtdbg.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace {

std::string g_dumpDir;
char g_dumpDirFixed[MAX_PATH * 2];
bool g_dumpDirFixedValid = false;
std::string g_appName;
std::string g_helpText;
std::vector<std::pair<std::string, std::string>> g_crashTypes;
bool g_cliMode = false;

// Forward whatever CrashTracker wrote to a pipe into our own console handle.
// Heap-safe: PeekNamedPipe/ReadFile/WriteFile + stack buffer only.
static void drainPipe(HANDLE hPipe, HANDLE hTarget)
{
    if (hPipe == INVALID_HANDLE_VALUE || hPipe == nullptr) return;
    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &avail, nullptr) || avail == 0)
            break;
        DWORD toRead = avail > sizeof(buf) ? (DWORD)sizeof(buf) : avail;
        DWORD rd = 0;
        if (!ReadFile(hPipe, buf, toRead, &rd, nullptr) || rd == 0)
            break;
        if (hTarget && hTarget != INVALID_HANDLE_VALUE) {
            DWORD wr = 0;
            WriteFile(hTarget, buf, rd, &wr, nullptr);
        }
    }
}

// Launch CrashTracker to view the generated dump. Heap-safe: only stack buffers
// and Win32 API (safe to call from the CRT hook heap-corruption path).
// In CLI mode: run CrashTracker --cli as a child with anonymous pipes for its
// stdout/stderr, then block (up to 30s) forwarding the report bytes into the
// current process's own console handles so the report lands in the SAME terminal
// that launched us (CrashTracker is a GUI-subsystem exe and would otherwise open
// a separate AllocConsole window). Non-CLI mode keeps the plain GUI launch.
static void launchCrashTracker(const char* dumpFile)
{
    char crashTool[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, crashTool, MAX_PATH);
    char* slash = len > 0 ? strrchr(crashTool, '\\') : nullptr;
    if (slash) strcpy(slash + 1, "CrashTracker.exe");
    else strcpy(crashTool, "CrashTracker.exe");

    char cmd[MAX_PATH * 4];
    if (g_cliMode)
        snprintf(cmd, sizeof(cmd), "\"%s\" --cli \"%s\"", crashTool, dumpFile);
    else
        snprintf(cmd, sizeof(cmd), "\"%s\" \"%s\"", crashTool, dumpFile);

    HANDLE hOutRead = INVALID_HANDLE_VALUE, hOutWrite = INVALID_HANDLE_VALUE;
    HANDLE hErrRead = INVALID_HANDLE_VALUE, hErrWrite = INVALID_HANDLE_VALUE;

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (g_cliMode) {
        SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
        if (CreatePipe(&hOutRead, &hOutWrite, &sa, 0)
            && CreatePipe(&hErrRead, &hErrWrite, &sa, 0)) {
            si.dwFlags = STARTF_USESTDHANDLES;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
            si.hStdOutput = hOutWrite;
            si.hStdError = hErrWrite;
        } else {
            if (hOutRead  != INVALID_HANDLE_VALUE) CloseHandle(hOutRead);
            if (hOutWrite != INVALID_HANDLE_VALUE) CloseHandle(hOutWrite);
            if (hErrRead  != INVALID_HANDLE_VALUE) CloseHandle(hErrRead);
            if (hErrWrite != INVALID_HANDLE_VALUE) CloseHandle(hErrWrite);
            hOutRead = hErrRead = hOutWrite = hErrWrite = INVALID_HANDLE_VALUE;
        }
    }

    BOOL inherit = g_cliMode ? TRUE : FALSE;
    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, inherit, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        if (g_cliMode && hOutRead != INVALID_HANDLE_VALUE) {
            CloseHandle(hOutWrite);
            CloseHandle(hErrWrite);

            HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
            HANDLE hStdErr = GetStdHandle(STD_ERROR_HANDLE);
            DWORD waited = 0;
            const DWORD stepMs = 100;
            const DWORD totalMs = 30000;
            for (;;) {
                drainPipe(hOutRead, hStdOut);
                drainPipe(hErrRead, hStdErr);
                DWORD rc = WaitForSingleObject(pi.hProcess, stepMs);
                if (rc == WAIT_OBJECT_0) break;
                if (rc == WAIT_TIMEOUT) {
                    waited += stepMs;
                    if (waited >= totalMs) break;
                } else {
                    break;
                }
            }
            drainPipe(hOutRead, hStdOut);
            drainPipe(hErrRead, hStdErr);
            CloseHandle(hOutRead);
            CloseHandle(hErrRead);
        }
        CloseHandle(pi.hProcess);
    } else {
        if (hOutRead  != INVALID_HANDLE_VALUE) CloseHandle(hOutRead);
        if (hOutWrite != INVALID_HANDLE_VALUE) CloseHandle(hOutWrite);
        if (hErrRead  != INVALID_HANDLE_VALUE) CloseHandle(hErrRead);
        if (hErrWrite != INVALID_HANDLE_VALUE) CloseHandle(hErrWrite);
    }
}

static std::string getExeDirectory()
{
    char buf[MAX_PATH];
    DWORD len = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (len > 0) {
        std::string path(buf, len);
        size_t pos = path.rfind('\\');
        if (pos != std::string::npos)
            return path.substr(0, pos);
    }
    return ".";
}

static void captureCallstack(const std::string& traceFile, EXCEPTION_POINTERS* exceptionInfo)
{
    if (!exceptionInfo || !exceptionInfo->ContextRecord) return;

    std::ofstream f(traceFile);
    if (!f.is_open()) return;

    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();

    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t slash = exeDir.rfind('\\');
    if (slash != std::string::npos) exeDir = exeDir.substr(0, slash);

    std::string searchPath = exeDir + ";" + exeDir + "\\modules\\*";

    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
    SymInitialize(hProcess, searchPath.c_str(), TRUE);

    CONTEXT ctx = *exceptionInfo->ContextRecord;
    STACKFRAME64 sf = {};
    DWORD machineType;
#ifdef _M_AMD64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    sf.AddrPC.Offset = ctx.Rip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Rbp;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Rsp;
    sf.AddrStack.Mode = AddrModeFlat;
#else
    machineType = IMAGE_FILE_MACHINE_I386;
    sf.AddrPC.Offset = ctx.Eip;
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Offset = ctx.Ebp;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Offset = ctx.Esp;
    sf.AddrStack.Mode = AddrModeFlat;
#endif

    for (int frame = 0; frame < 64; ++frame) {
        if (!StackWalk64(machineType, hProcess, hThread, &sf, &ctx,
            nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
        if (sf.AddrPC.Offset == 0) break;

        IMAGEHLP_MODULE64 modInfo = { sizeof(IMAGEHLP_MODULE64) };
        std::string modName = "?";
        DWORD64 modBase = 0;
        if (SymGetModuleInfo64(hProcess, sf.AddrPC.Offset, &modInfo)) {
            modName = modInfo.ModuleName;
            modBase = modInfo.BaseOfImage;
        }

        char symBuf[sizeof(SYMBOL_INFO) + 512] = {};
        SYMBOL_INFO* sym = (SYMBOL_INFO*)symBuf;
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 511;
        DWORD64 disp = 0;
        std::string symName;
        if (SymFromAddr(hProcess, sf.AddrPC.Offset, &disp, sym))
            symName = sym->Name;
        else
            symName = "(unknown)";

        DWORD64 offset = sf.AddrPC.Offset - modBase;
        f << std::hex << sf.AddrPC.Offset << "|" << modName << "|0x" << offset
          << "|" << symName << "+0x" << disp << "\n";
    }

    SymCleanup(hProcess);
    f.close();
}

static void writeDumpAndLaunch(EXCEPTION_POINTERS* exceptionInfo)
{
    SYSTEMTIME st;
    GetLocalTime(&st);

    std::ostringstream stem;
    stem << st.wYear
         << std::setfill('0')
         << std::setw(2) << st.wMonth
         << std::setw(2) << st.wDay << "_"
         << std::setw(2) << st.wHour
         << std::setw(2) << st.wMinute
         << std::setw(2) << st.wSecond;

    std::string timestamp = stem.str();
    std::string reportDir = g_dumpDir + "\\crash-report\\" + timestamp;
    CreateDirectoryA((g_dumpDir + "\\crash-report").c_str(), nullptr);
    CreateDirectoryA(reportDir.c_str(), nullptr);

    std::string dumpFile = reportDir + "\\crash_" + timestamp + ".dmp";
    std::string traceFile = reportDir + "\\crash_" + timestamp + ".trace";

    HANDLE hFile = CreateFileA(dumpFile.c_str(), GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = exceptionInfo;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
            MiniDumpNormal, exceptionInfo ? &mei : nullptr, nullptr, nullptr);
        CloseHandle(hFile);

        captureCallstack(traceFile, exceptionInfo);

        std::string metaFile = reportDir + "\\crash_" + timestamp + ".meta";
        std::ofstream meta(metaFile);
        meta << g_appName << std::endl;
        if (!g_helpText.empty())
            meta << g_helpText;
        if (!g_crashTypes.empty()) {
            meta << "\n" << "===CRASH_DESCRIPTIONS===" << "\n";
            for (auto& ct : g_crashTypes) {
                meta << ct.first << "\n" << ct.second << "\n===END===\n";
            }
        }
        meta.close();

        std::string sigPath = reportDir + "\\crash_signal.txt";
        std::ofstream sig(sigPath);
        sig << dumpFile << "\n" << traceFile << std::endl;
        sig.close();

        launchCrashTracker(dumpFile.c_str());
    }
}

LONG WINAPI NeoUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    writeDumpAndLaunch(exceptionInfo);
    DWORD exitCode = 0xC0000409;
    if (exceptionInfo && exceptionInfo->ExceptionRecord)
        exitCode = exceptionInfo->ExceptionRecord->ExceptionCode;
    TerminateProcess(GetCurrentProcess(), exitCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

#ifdef _DEBUG
// CRT report hook: captures HEAP_CORRUPTION / _CrtIsValidHeapPointer /
// __acrt_first_block asserts. These do NOT go through SetUnhandledExceptionFilter,
// so they must be caught via _CrtSetReportHookW2.
// The heap may already be corrupt here: this path must NOT allocate heap memory
// (no new / STL containers / iostream). Use stack buffers + Win32 API only.
static void writeCrtDumpHeapSafe(const wchar_t* message)
{
    static bool inHook = false;
    if (inHook) return;
    inHook = true;

    SYSTEMTIME st;
    GetLocalTime(&st);

    char ts[64];
    snprintf(ts, sizeof(ts), "%04u%02u%02u_%02u%02u%02u",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    char crashRoot[MAX_PATH * 2];
    char reportDir[MAX_PATH * 2];
    snprintf(crashRoot, sizeof(crashRoot), "%s\\crash-report",
        g_dumpDirFixedValid ? g_dumpDirFixed : ".");
    CreateDirectoryA(crashRoot, nullptr);
    snprintf(reportDir, sizeof(reportDir), "%s\\%s", crashRoot, ts);
    CreateDirectoryA(reportDir, nullptr);

    char dumpFile[MAX_PATH * 2];
    char traceFile[MAX_PATH * 2];
    char metaFile[MAX_PATH * 2];
    char sigFile[MAX_PATH * 2];
    snprintf(dumpFile, sizeof(dumpFile), "%s\\crash_%s.dmp", reportDir, ts);
    snprintf(traceFile, sizeof(traceFile), "%s\\crash_%s.trace", reportDir, ts);
    snprintf(metaFile, sizeof(metaFile), "%s\\crash_%s.meta", reportDir, ts);
    snprintf(sigFile, sizeof(sigFile), "%s\\crash_signal.txt", reportDir, ts);

    CONTEXT ctx;
    RtlCaptureContext(&ctx);
    EXCEPTION_RECORD er = {};
    er.ExceptionCode = 0xC0000409; // STATUS_STACK_BUFFER_OVERRUN (assert simulation)
    er.ExceptionAddress = (PVOID)ctx.Rip;
    EXCEPTION_POINTERS ep = { &er, &ctx };

    HANDLE hFile = CreateFileA(dumpFile, GENERIC_WRITE, 0,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = GetCurrentThreadId();
        mei.ExceptionPointers = &ep;
        mei.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
            MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    {
        HANDLE hTrace = CreateFileA(traceFile, GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hTrace != INVALID_HANDLE_VALUE) {
            void* frames[64] = {};
            USHORT n = CaptureStackBackTrace(0, 64, frames, nullptr);
            char line[256];
            for (USHORT i = 0; i < n; ++i) {
                int len = snprintf(line, sizeof(line), "0x%016llx|?|0x0|frame_%u+0x0\n",
                    (unsigned long long)(uintptr_t)frames[i], (unsigned)i);
                DWORD written = 0;
                WriteFile(hTrace, line, (DWORD)len, &written, nullptr);
            }
            CloseHandle(hTrace);
        }
    }

    {
        HANDLE hMeta = CreateFileA(metaFile, GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hMeta != INVALID_HANDLE_VALUE) {
            auto wstr = [&](const char* s) {
                DWORD written = 0;
                WriteFile(hMeta, s, (DWORD)strlen(s), &written, nullptr);
            };
            wstr(g_appName.empty() ? "Crash" : g_appName.c_str());
            wstr("\n");
            if (message) {
                char mbuf[2048];
                int len = WideCharToMultiByte(CP_UTF8, 0, message, -1,
                    mbuf, sizeof(mbuf), nullptr, nullptr);
                if (len > 0) {
                    wstr("===CRT_MESSAGE===\n");
                    wstr(mbuf);
                    wstr("\n");
                }
            }
            if (!g_helpText.empty()) {
                wstr("===CRT_HELP===\n");
                wstr(g_helpText.c_str());
                wstr("\n");
            }
            wstr("===CRASH_DESCRIPTIONS===\n");
            wstr("CRT_ASSERT\n");
            wstr("CRT debug heap detected a problem (HEAP CORRUPTION or invalid heap pointer).\n");
            wstr("===END===\n");
            CloseHandle(hMeta);
        }
    }

    {
        char sig[MAX_PATH * 4];
        snprintf(sig, sizeof(sig), "%s\n%s\n", dumpFile, traceFile);
        HANDLE hSig = CreateFileA(sigFile, GENERIC_WRITE, 0,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hSig != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(hSig, sig, (DWORD)strlen(sig), &written, nullptr);
            CloseHandle(hSig);
        }
    }

    launchCrashTracker(dumpFile);

    TerminateProcess(GetCurrentProcess(), 0xC0000374);

    inHook = false;
}

static int __cdecl crtReportHook(int reportType, wchar_t* message, int* returnValue)
{
    if (reportType == _CRT_ASSERT || reportType == _CRT_ERROR) {
        writeCrtDumpHeapSafe(message);
    }
    return FALSE; // let CRT continue default handling (dialog/abort)
}
#endif // _DEBUG
} // anonymous namespace
#endif

namespace HiBerCTM {

void InstallCrashHandler(const std::string& dumpDir)
{
#ifdef _WIN32
    g_dumpDir = dumpDir.empty() ? getExeDirectory() : dumpDir;
    snprintf(g_dumpDirFixed, sizeof(g_dumpDirFixed), "%s", g_dumpDir.c_str());
    g_dumpDirFixedValid = true;

    ULONG guarantee = 65536;
    SetThreadStackGuarantee(&guarantee);

    SetUnhandledExceptionFilter(NeoUnhandledExceptionFilter);
#endif
}

void InstallCrtReportHook()
{
#ifdef _WIN32
#ifdef _DEBUG
    _CrtSetReportHookW2(_CRT_RPTHOOK_INSTALL, crtReportHook);
#endif
#endif
}

void SetCrashAppName(const std::string& name)
{
    g_appName = name;
}

void SetCrashHelpText(const std::string& text)
{
    g_helpText = text;
}

void AddCrashTypeInfo(const std::string& name, const std::string& description)
{
    g_crashTypes.push_back({name, description});
}

void SetCrashCliMode(bool enabled)
{
    g_cliMode = enabled;
}

} // namespace HiBerCTM
