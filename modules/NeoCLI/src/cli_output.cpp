#include "cli_output.h"

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <cmath>

#include <nlohmann/json.hpp>

namespace NeoCLI {

bool CliOutput::quiet_ = false;
bool CliOutput::verbose_ = false;
bool CliOutput::jsonMode_ = false;

bool CliOutput::useColors()
{
    static bool checked = false;
    static bool colorSupported = false;
    if (!checked) {
        checked = true;
        colorSupported = (ISATTY(FILENO(stdout)) != 0);
#ifdef _WIN32
        if (colorSupported) {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            if (GetConsoleMode(hOut, &mode)) {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
        }
#endif
    }
    return colorSupported;
}

static const char* RESET  = "\033[0m";
static const char* GREEN  = "\033[1;32m";
static const char* YELLOW = "\033[1;33m";
static const char* RED    = "\033[1;31m";
static const char* CYAN   = "\033[1;36m";
static const char* BOLD   = "\033[1m";
static const char* DIM    = "\033[2m";

static std::ostream& out()
{
    return CliOutput::isJsonMode() ? std::cerr : std::cout;
}

void CliOutput::info(const std::string& msg)
{
    if (quiet_) return;
    if (useColors())
        out() << DIM << "[*] " << RESET << msg << std::endl;
    else
        out() << "[*] " << msg << std::endl;
}

void CliOutput::success(const std::string& msg)
{
    if (quiet_) return;
    if (useColors())
        out() << GREEN << "[+] " << msg << RESET << std::endl;
    else
        out() << "[+] " << msg << std::endl;
}

void CliOutput::warning(const std::string& msg)
{
    if (quiet_) return;
    if (useColors())
        std::cerr << YELLOW << "[!] " << msg << RESET << std::endl;
    else
        std::cerr << "[!] " << msg << std::endl;
}

void CliOutput::error(const std::string& msg)
{
    if (useColors())
        std::cerr << RED << "[-] " << msg << RESET << std::endl;
    else
        std::cerr << "[-] " << msg << std::endl;
}

void CliOutput::progress(int percent, const std::string& msg)
{
    if (quiet_) return;
    int p = (std::min)(100, (std::max)(0, percent));
    int barWidth = 30;
    int filled = (p * barWidth) / 100;
    int empty = barWidth - filled;

    std::string bar;
    bar.reserve(static_cast<size_t>(barWidth) + 3);
    bar += '[';
    for (int i = 0; i < filled; ++i) bar += '=';
    if (p < 100 && filled < barWidth) {
        bar += '>';
        --empty;
    }
    for (int i = 0; i < empty; ++i) bar += ' ';
    bar += ']';

char buf[512];
    std::snprintf(buf, sizeof(buf), "\r%s %3d%% | %s",
        bar.c_str(), p, msg.c_str());

    out() << buf << std::flush;
    if (p >= 100) {
        out() << std::endl;
    }
}

void CliOutput::table(const std::vector<std::string>& headers,
    const std::vector<std::vector<std::string>>& rows)
{
    if (quiet_) return;

    std::vector<size_t> widths(headers.size(), 0);
    for (size_t i = 0; i < headers.size(); ++i) {
        widths[i] = headers[i].size();
    }
    for (const auto& row : rows) {
        for (size_t i = 0; i < row.size() && i < widths.size(); ++i) {
            widths[i] = (std::max)(widths[i], row[i].size());
        }
    }

    auto printRow = [&](const std::vector<std::string>& cells, bool header) {
        out() << (useColors() && header ? BOLD : "");
        for (size_t i = 0; i < cells.size(); ++i) {
            out() << " " << std::left << std::setw(static_cast<int>(widths[i]))
                  << cells[i];
            if (i + 1 < cells.size()) {
                out() << (useColors() ? DIM : "") << " |" << RESET;
            }
        }
        if (useColors() && header) out() << RESET;
        out() << std::endl;
    };

    if (!headers.empty()) {
        printRow(headers, true);
        std::string sep;
        for (size_t i = 0; i < widths.size(); ++i) {
            sep += std::string(widths[i] + 1, '-');
            if (i + 1 < widths.size()) sep += "-+-";
        }
        if (useColors())
            out() << DIM << sep << RESET << std::endl;
        else
            out() << sep << std::endl;
    }

    for (const auto& row : rows) {
        printRow(row, false);
    }
}

void CliOutput::separator(char c, int width)
{
    if (quiet_) return;
    if (useColors())
        out() << DIM << std::string(width, c) << RESET << std::endl;
    else
        out() << std::string(width, c) << std::endl;
}

void CliOutput::title(const std::string& title)
{
    if (quiet_) return;
    size_t len = title.size();
    size_t side = (len > 58) ? 1 : (60 - len) / 2;
    if (useColors())
        out() << DIM << std::string(side, '=') << RESET
              << " " << BOLD << title << RESET << " "
              << DIM << std::string(side, '=') << RESET << std::endl;
    else
        out() << std::string(side, '=') << " " << title << " "
              << std::string(side, '=') << std::endl;
}

void CliOutput::jsonBlock(const nlohmann::json& value)
{
    std::cout << "=====JSON-BEGIN=====\n";
    std::cout << value.dump(2) << "\n";
    std::cout << "=====JSON-END=====\n";
    std::cout << std::flush;
}

void CliOutput::setQuiet(bool quiet) { quiet_ = quiet; }
void CliOutput::setVerbose(bool verbose) { verbose_ = verbose; }
bool CliOutput::isQuiet() { return quiet_; }

void CliOutput::setJsonMode(bool on) { jsonMode_ = on; }
bool CliOutput::isJsonMode() { return jsonMode_; }

} // namespace NeoCLI

