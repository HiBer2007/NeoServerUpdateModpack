#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

// 独立日志系统 (CommonLoggerCPP): 类 CLogger, 全局命名空间 (调用即 CLogger::Xxx)
class CLogger {
public:
    enum class Level { Trace = 0, Debug, Info, Warn, Error, Off };

    static void Init(const std::string& logFile = "core.log",
        const std::string& loggerName = "core");

    static std::shared_ptr<spdlog::logger> Get() { return instance_; }

    // 空指针安全: instance_ 为空(如插件 DLL 内未 Init)时回退 spdlog 共享 registry 的
    // default_logger (宿主 Init 后经 set_default_logger 注入), 保证跨模块日志可达。
    static std::shared_ptr<spdlog::logger> Resolve();

    static void SetLevel(Level level);
    static void AddSink(const std::shared_ptr<spdlog::sinks::sink>& sink);

    template<typename... Args>
    static void Trace(const char* fmt, Args&&... args) {
        auto l = Resolve();
        if (l) l->trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(const char* fmt, Args&&... args) {
        auto l = Resolve();
        if (l) l->info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(const char* fmt, Args&&... args) {
        auto l = Resolve();
        if (l) l->error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(const char* fmt, Args&&... args) {
        auto l = Resolve();
        if (l) l->warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(const char* fmt, Args&&... args) {
        auto l = Resolve();
        if (l) l->debug(fmt, std::forward<Args>(args)...);
    }

private:
    static std::shared_ptr<spdlog::logger> instance_;
};
