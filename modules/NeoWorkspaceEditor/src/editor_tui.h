#pragma once

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <memory>

#include <spdlog/sinks/sink.h>
#include <spdlog/details/log_msg.h>

namespace nsum_tui {

// 加载期日志缓冲 sink: 拦截 CLogger 的 stdout 输出, 缓冲保存纯文本行
// (无 ANSI 颜色, 无尾部换行), 渲染时原样输出 (2026-08-09 用户放弃 TUI 日志颜色)
class TuiLogSink final : public spdlog::sinks::sink {
public:
    TuiLogSink();

    void log(const spdlog::details::log_msg& msg) override;
    void flush() override {}
    void set_pattern(const std::string&) override;
    void set_formatter(std::unique_ptr<spdlog::formatter>) override;

    std::vector<std::string> snapshot(size_t maxLines) const;

private:
    mutable std::mutex mtx_;
    std::deque<std::string> lines_;
};

// 编辑器加载期叠层 TUI:
//  - 上层: 居中 NSUM Editor 大型拼接字(亮青) + 灰色实心卡片背景
//  - 下层: 日志滚动区 (来自 TuiLogSink 缓冲, 带原生级别色)
//  - 底部: 进度条(左) + 阶段文本(右)
// 终端/双击启动: start() 拦截 stdout sink + 启动渲染线程; 主窗口展示后 stop()
// 恢复 stdout sink 并重放缓冲日志, 之后日志恢复正常输出
class EditorTui {
public:
    EditorTui();
    ~EditorTui();

    void start();
    void stop();
    void setStatus(const std::string& stage);
    void setProgress(int percent);

private:
    void renderLoop();
    std::string renderFrame(int width, int height);

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<size_t> frame_{0};

    mutable std::mutex statusMutex_;
    std::string stage_;
    std::atomic<int> targetPercent_{0};

    std::shared_ptr<TuiLogSink> bufSink_;
    std::shared_ptr<spdlog::sinks::sink> savedStdoutSink_;
};

} // namespace nsum_tui
