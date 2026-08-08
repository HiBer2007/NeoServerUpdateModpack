cmake_minimum_required(VERSION 3.20)
add_library(CrashTrackerHandleLib STATIC
    src/crash_reporter.cpp
)
target_include_directories(CrashTrackerHandleLib PUBLIC include)
target_link_libraries(CrashTrackerHandleLib PUBLIC dbghelp)
if(MSVC)
    target_compile_definitions(CrashTrackerHandleLib PRIVATE _WIN32)
endif()
