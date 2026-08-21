#include "editor_tui.h"

#include <cstdio>
#include <chrono>
#include <thread>
#include <algorithm>
#include <ctime>

#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/details/os.h>

#include <logger.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace nsum_tui {

namespace {

const char* kEsc = "\x1b[";
const char* kGrayBg = "\x1b[48;2;52;56;62m";    // 灰色实心卡片背景
const char* kCyan = "\x1b[38;2;96;216;255m";    // 拼接字亮青
const char* kWhite = "\x1b[38;2;230;233;240m";
const char* kGreenBg = "\x1b[42m";              // 进度条已填充
const char* kDarkBg = "\x1b[100m";              // 进度条未填充 (亮灰底)
const char* kReset = "\x1b[0m";

std::string colored(const char* fg, const std::string& s)
{
    return std::string(fg) + s + kReset;
}

std::string bgPad(int n)
{
    std::string s;
    s.reserve(static_cast<size_t>(n) * 4);
    for (int i = 0; i < n; ++i) {
        s += " ";
    }
    return s;
}

int termWidth()
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != nullptr && h != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(h, &info)) {
            return static_cast<int>(info.dwSize.X);
        }
    }
#endif
    return 100;
}

int termHeight()
{
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h != nullptr && h != INVALID_HANDLE_VALUE) {
        CONSOLE_SCREEN_BUFFER_INFO info;
        if (GetConsoleScreenBufferInfo(h, &info)) {
            return static_cast<int>(info.dwSize.Y);
        }
    }
#endif
    return 30;
}

const char* kArt[] = {
    "    _   _______ __  ____  ___   ______    ___ __",
    "   / | / / ___// / / /  |/  /  / ____/___/ (_) /_____  _____",
    "  /  |/ /\\__ \\/ / / / /|_/ /  / __/ / __  / / __/ __ \\/ ___/",
    " / /|  /___/ / /_/ / /  / /  / /___/ /_/ / / /_/ /_/ / /",
    "/_/ |_//____/\\____/_/  /_/  /_____/\\__,_/_/\\__/\\____/_/ ",
};
const int kArtLines = 5;
const int kArtMaxLen = 60;  // 最长 art 行宽 (行 1/2): 各行自带前导空格是图案一部分,
                            // 渲染时统一左缩进居中, 保持各行原始缩进对齐
const int kArtWidth = kArtMaxLen;   // 卡片内容区宽度 = art 最大行宽

} // namespace

TuiLogSink::TuiLogSink()
{
}

void TuiLogSink::set_pattern(const std::string&)
{
}

void TuiLogSink::set_formatter(std::unique_ptr<spdlog::formatter>)
{
}

void TuiLogSink::log(const spdlog::details::log_msg& msg)
{
    char ts[16];
    const auto t = spdlog::details::os::localtime(
        std::chrono::system_clock::to_time_t(msg.time));
    std::snprintf(ts, sizeof(ts), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);

    const char* lvl = "info";
    switch (msg.level) {
        case spdlog::level::trace: lvl = "trace"; break;
        case spdlog::level::debug: lvl = "debug"; break;
        case spdlog::level::info:  lvl = "info";  break;
        case spdlog::level::warn:  lvl = "warn";  break;
        case spdlog::level::err:   lvl = "error"; break;
        case spdlog::level::critical: lvl = "crit"; break;
        default: break;
    }

    // 纯文本行, 无 ANSI 颜色, 无尾部换行 (渲染时统一加 \r\n)
    std::string line = std::string("[") + ts + "] [" + lvl + "] "
        + std::string(msg.payload.data(), msg.payload.size());

    std::lock_guard<std::mutex> lk(mtx_);
    lines_.push_back(std::move(line));
    if (lines_.size() > 2000) {
        lines_.pop_front();
    }
}

std::vector<std::string> TuiLogSink::snapshot(size_t maxLines) const
{
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::string> out;
    const size_t start = (lines_.size() > maxLines)
        ? lines_.size() - maxLines : 0;
    for (size_t i = start; i < lines_.size(); ++i) {
        out.push_back(lines_[i]);
    }
    return out;
}

EditorTui::EditorTui()
{
}

EditorTui::~EditorTui()
{
    stop();
}

void EditorTui::setStatus(const std::string& stage)
{
    std::lock_guard<std::mutex> lk(statusMutex_);
    stage_ = stage;
}

void EditorTui::setProgress(int percent)
{
    targetPercent_.store(percent);
}

void EditorTui::start()
{
    auto lg = CLogger::Get();
    if (!lg) {
        return;
    }

    auto& sinks = lg->sinks();
    for (auto it = sinks.begin(); it != sinks.end(); ++it) {
        if (dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(it->get())) {
            savedStdoutSink_ = *it;
            sinks.erase(it);
            break;
        }
    }

    bufSink_ = std::make_shared<TuiLogSink>();
    lg->sinks().push_back(bufSink_);

    running_.store(true);
    thread_ = std::thread([this]() { renderLoop(); });
}

void EditorTui::stop()
{
    running_.store(false);
    if (thread_.joinable()) {
        thread_.join();
    }
    if (!bufSink_) {
        return;
    }

    auto lg = CLogger::Get();
    if (lg) {
        auto& sinks = lg->sinks();
        for (auto it = sinks.begin(); it != sinks.end(); ++it) {
            if (*it == bufSink_) {
                sinks.erase(it);
                break;
            }
        }
        if (savedStdoutSink_) {
            sinks.insert(sinks.begin(), savedStdoutSink_);
            savedStdoutSink_.reset();
        }
    }

    const auto lines = bufSink_->snapshot(2000);
    std::string out = "\x1b[2J\x1b[H\x1b[?25h";
    for (const auto& l : lines) {
        out += l;
        out += "\n";
    }
    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);
    bufSink_.reset();
}

// 日志行着色: 行格式 "[HH:MM:SS] \x01[level]\x02 消息"
//  [时间] 灰 / [level] 按级别着色 (info 绿 warn 黄 error 红 debug 灰) / 消息浅灰
std::string EditorTui::renderFrame(int width, int height)
{
    std::string out;
    out.reserve(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
    out += "\x1b[?25l\x1b[2J\x1b[H";

    // ---- 卡片几何: 仅矩形范围灰底, 水平+垂直居中 (每帧实时按终端尺寸计算) ----
    const int cardPadX = 4;
    const int cardW = kArtWidth + cardPadX * 2;
    const int cardH = kArtLines + 3;              // 上留白 + art + 版本行 + 下留白
    const bool hideCard = (width < cardW + 8) || (height < cardH + 4);
    const int cardLeft = (std::max)(0, (width - cardW) / 2);
    const int cardTop = (std::max)(0, (height - cardH) / 2);

    // ---- 底部状态行 (占最后一行) ----
    std::string stage;
    {
        std::lock_guard<std::mutex> lk(statusMutex_);
        stage = stage_;
    }
    const int bodyH = (std::max)(0, height - 1);

    // ---- 日志: 底部对齐滚动 —— 取缓冲最后 bodyH 行, 最新行紧贴状态栏上方,
    //      旧日志向上排列, 行数不足时顶部留空, 溢出时上方自然丢弃;
    //      卡片固定垂直居中覆盖, 不被日志挤动 ----
    const auto logs = bufSink_ ? bufSink_->snapshot(static_cast<size_t>(bodyH))
                               : std::vector<std::string>();
    const size_t logStartRow = (logs.size() < static_cast<size_t>(bodyH))
        ? bodyH - static_cast<int>(logs.size()) : 0;
    for (int row = 0; row < bodyH; ++row) {
        const bool inCard = !hideCard
            && row >= cardTop && row < cardTop + cardH;
        if (inCard) {
            const int ci = row - cardTop;
            out += std::string(static_cast<size_t>(cardLeft), ' ');   // 卡外默认背景
            out += kGrayBg;                                           // 卡内灰底
            if (ci == 0 || ci == cardH - 1) {
                out += std::string(static_cast<size_t>(cardW), ' ');
            } else if (ci <= kArtLines) {
                // 统一左缩进: 各行自带前导空格是 figlet 图案一部分, 按最长行
                // 宽度整体居中, 保持各行原始缩进对齐 (每行各自居中会错位);
                // 右缘灰底按实际行宽补齐, 保证卡片矩形完整
                const std::string& art = kArt[ci - 1];
                const int pad = (std::max)(0, (cardW - kArtMaxLen) / 2);
                out += std::string(static_cast<size_t>(pad), ' ');
                out += kCyan;
                out += art;
                out += kReset;
                out += kGrayBg;
                out += std::string(static_cast<size_t>(
                    (std::max)(0, cardW - pad - static_cast<int>(art.size()))), ' ');
            } else {
                const std::string ver = "NeoServerUpdateModpack Editor v1.0.0";
                const int pad = (std::max)(0, (cardW - static_cast<int>(ver.size())) / 2);
                out += std::string(static_cast<size_t>(pad), ' ');
                out += kWhite;
                out += ver;
                out += kReset;
                out += kGrayBg;
                out += std::string(static_cast<size_t>(
                    (std::max)(0, cardW - pad - static_cast<int>(ver.size()))), ' ');
            }
            out += kReset;
            out += std::string(static_cast<size_t>(
                (std::max)(0, width - cardLeft - cardW)), ' ');         // 卡外默认背景
            out += "\r\n";
            continue;
        }
        if (row >= static_cast<int>(logStartRow)) {
            const size_t li = row - logStartRow;
            if (li < logs.size()) {
                std::string view = logs[li];
                if (view.size() > static_cast<size_t>(width) - 1) {
                    view = view.substr(0, static_cast<size_t>(width) - 4);
                    view += "...";
                }
                out += view;
            }
        }
        out += "\r\n";
    }

    // ---- 底部: 进度条(左, 无括号) + 阶段文本(右), 默认背景 ----
    // 直接映射 targetPercent_ (main 的 hold 分片 setProgress 自带渐进效果,
    // 渲染线程不做二次平滑 —— 平滑爬升 +2/帧在短生命周期内永远追不上目标)
    const int barW = 24;
    const int target = targetPercent_.load();
    const int done = barW * target / 100;
    std::string bar;
    bar.reserve(static_cast<size_t>(barW) * 8);
    for (int i = 0; i < barW; ++i) {
        if (i < done) {
            bar += std::string(kGreenBg) + " " + kReset;
        } else {
            bar += std::string(kDarkBg) + " " + kReset;
        }
    }
    out += bar;
    out += " ";
    const int stageLeft = (std::max)(0,
        width - 1 - static_cast<int>(stage.size()));
    if (stageLeft > static_cast<int>(bar.size()) + 1) {
        out += std::string(static_cast<size_t>(
            stageLeft - static_cast<int>(bar.size()) - 1), ' ');
    }
    out += std::string(kWhite) + stage + kReset;
    out += "\r\n";

    return out;
}

void EditorTui::renderLoop()
{
    while (running_.load()) {
        const int w = termWidth();
        const int h = termHeight();
        const std::string frame = renderFrame(w, h);
        std::fwrite(frame.data(), 1, frame.size(), stdout);
        std::fflush(stdout);
        frame_.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

} // namespace nsum_tui
