cmake_minimum_required(VERSION 3.20)
add_executable(CrashTracker WIN32
    src/main.cpp
    src/crash_window.cpp
    src/app.rc
)
target_include_directories(CrashTracker PRIVATE src)
target_link_libraries(CrashTracker PRIVATE Qt6::Core Qt6::Widgets dbghelp)

nsum_add_version_info(CrashTracker
    "HiBer2007系列软件通用崩溃追踪工具"
    "NSUM构建工具")
