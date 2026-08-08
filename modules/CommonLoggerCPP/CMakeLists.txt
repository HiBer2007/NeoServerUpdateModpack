cmake_minimum_required(VERSION 3.20)

# ============================================================
# CommonLoggerCPP: 独立日志系统 (从 NeoCore 提取, 2026-08-07)
#  - NeoCore::Logger 静态类: spdlog 封装 (stdout+file sink)
#  - 跨 DLL 日志共享: Resolve() 空指针回退 spdlog::default_logger()
#  - 插件日志注册: plugin_log_sink.h (ILogSink + NEO_DECLARE_PLUGIN_LOG_SINK 宏)
# 命名空间沿用 NeoCore (兼容既有 400+ 调用点), 独立构建单元可复用
# ============================================================
add_library(CommonLoggerCPP STATIC
    src/logger.cpp
        include/logger.h
        include/plugin_log_sink.h
)
target_include_directories(CommonLoggerCPP
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(CommonLoggerCPP
    PUBLIC
        spdlog::spdlog
)
