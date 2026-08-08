#pragma once

#include <string>

#include <logger.h>

// 插件日志注册机制 (导出符号约定, 不依赖任何固定数量接口):
//
//   - 插件 DLL 内使用宏 NEO_DECLARE_PLUGIN_LOG_SINK("插件名") 一次性展开:
//       * 静态 ILogSink* g_pluginSink
//       * extern "C" __declspec(dllexport) void SetPluginLogSink(ILogSink*)   <- 宿主注入点
//       * PluginLog(level, msg) 便捷函数: g_pluginSink 非空走富接口(带插件名前缀),
//         为空回退 CLogger (经 spdlog default_logger 跨模块到达宿主日志)
//   - 宿主加载点 (PluginLoader / ModpackExporter::loadExporter / PointerDownloader)
//     统一 GetProcAddress("SetPluginLogSink") 注入宿主实现的 ILogSink;
//     找不到则跳过 (旧插件/第三方插件兼容, 靠 CLogger 回退通道仍可输出日志)
//
// 用法:
//   #include <plugin_log_sink.h>
//   NEO_DECLARE_PLUGIN_LOG_SINK("NeoExporter_MCBBS")

// 宿主注入的日志回调接口: level 0=Trace 1=Debug 2=Info 3=Warn 4=Error
class ILogSink {
public:
    virtual ~ILogSink() = default;
    virtual void log(int level, const std::string& message,
        const char* pluginName) = 0;
};

// 宿主默认实现: 转发到 CLogger (带插件名前缀)
class LoggerLogSink : public ILogSink {
public:
    void log(int level, const std::string& message,
        const char* pluginName) override
    {
        if (!pluginName || !*pluginName)
            pluginName = "plugin";
        switch (level) {
        case 0: CLogger::Trace("[{}] {}", pluginName, message); break;
        case 1: CLogger::Debug("[{}] {}", pluginName, message); break;
        case 3: CLogger::Warn("[{}] {}", pluginName, message); break;
        case 4: CLogger::Error("[{}] {}", pluginName, message); break;
        default: CLogger::Info("[{}] {}", pluginName, message); break;
        }
    }
};

#define NEO_DECLARE_PLUGIN_LOG_SINK(PLUGIN_NAME)                                \
    static ILogSink* g_pluginSink = nullptr;                                   \
    extern "C" __declspec(dllexport) void SetPluginLogSink(                    \
        ILogSink* sink)                                                        \
    {                                                                          \
        g_pluginSink = sink;                                                   \
    }                                                                          \
    inline void PluginLog(int level, const std::string& msg)                   \
    {                                                                          \
        if (g_pluginSink)                                                      \
            g_pluginSink->log(level, msg, PLUGIN_NAME);                        \
        else {                                                                 \
            switch (level) {                                                   \
            case 0: CLogger::Trace("{}", msg); break;                          \
            case 1: CLogger::Debug("{}", msg); break;                          \
            case 3: CLogger::Warn("{}", msg); break;                           \
            case 4: CLogger::Error("{}", msg); break;                          \
            default: CLogger::Info("{}", msg); break;                          \
            }                                                                  \
        }                                                                      \
    }
