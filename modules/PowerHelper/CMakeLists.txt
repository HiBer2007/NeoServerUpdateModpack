cmake_minimum_required(VERSION 3.20)

# 独立 EXE 模式 (静态 Qt, POWERHELPER_STANDALONE_ONLY=ON 独立预设)
option(POWERHELPER_STANDALONE_ONLY "PowerHelper standalone static-Qt EXE build" OFF)

if(POWERHELPER_STANDALONE_ONLY)
    set(PH_CORE_LINKAGE STATIC)
    add_compile_definitions(POWERHELPER_STATIC)
else()
    set(PH_CORE_LINKAGE SHARED)
endif()

find_package(cmark-gfm REQUIRED)
find_package(cmark-gfm-extensions REQUIRED)
if(WIN32)
    find_package(unofficial-webview2 CONFIG REQUIRED)
endif()

# ============================================================
# PowerHelperCore: 阅读器核心 (默认 SHARED; 独立模式 STATIC)
# ============================================================
add_library(PowerHelperCore ${PH_CORE_LINKAGE}
    core/src/markdown_renderer.cpp
    core/src/markdown_viewer.cpp
    core/src/doc_group.cpp
        core/include/markdown_renderer.h
        core/include/markdown_viewer.h
        core/include/doc_group.h
        core/include/powerhelper_export.h
)
target_include_directories(PowerHelperCore
    PUBLIC  core/include
    PRIVATE core/src
)
target_link_libraries(PowerHelperCore
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        libcmark-gfm
        libcmark-gfm-extensions
)
if(WIN32)
    target_link_libraries(PowerHelperCore
        PUBLIC unofficial::webview2::webview2
    )
endif()

# ============================================================
# PowerHelperBridge: 集成工具 (STATIC, PowerHelperBridgeLibrary)
# ============================================================
add_library(PowerHelperBridge STATIC
    bridge/src/powerhelper_bridge.cpp
        bridge/include/powerhelper_bridge.h
)
target_include_directories(PowerHelperBridge
    PUBLIC  bridge/include
    PRIVATE bridge/src
)
target_link_libraries(PowerHelperBridge
    PUBLIC Qt6::Core Qt6::Widgets
)

# ============================================================
# PowerHelper: 外壳 EXE (GUI 阅读器 + CLI)
# 控制台子系统 (与主程序同款): CLI 模式 stdout 原生连接控制台;
# GUI 模式在 main 内 AttachConsole 失败时 FreeConsole 释放终端
# ============================================================
add_executable(PowerHelper
    app/main.cpp
    app/powerhelper_cli.cpp
    app/powerhelper_cli.h
    app/reader_window.cpp
    app/reader_window.h
)
target_include_directories(PowerHelper
    PRIVATE app
    PRIVATE core/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../CrashTrackerHandleLib/include
)
target_link_libraries(PowerHelper
    PRIVATE
        PowerHelperCore
        PowerHelperBridge
        NeoCore
        CrashTrackerHandleLib
        Qt6::Core
        Qt6::Widgets
)
nsum_add_version_info(PowerHelper
    "PowerHelper - Markdown 文档阅读器"
    "PowerHelper"
    "${CMAKE_SOURCE_DIR}/PR.ico")

if(WIN32 AND NOT POWERHELPER_STANDALONE_ONLY)
    # 常规模式: 核心 DLL 随外壳 EXE 部署
    add_custom_command(TARGET PowerHelper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:PowerHelperCore>" "$<TARGET_FILE_DIR:PowerHelper>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libcmark-gfm>" "$<TARGET_FILE_DIR:PowerHelper>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libcmark-gfm-extensions>" "$<TARGET_FILE_DIR:PowerHelper>/"
        COMMENT "PowerHelper: deploy core dlls"
    )
    get_target_property(_ph_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_ph_qmake)
        get_filename_component(_ph_qt_bin "${_ph_qmake}" DIRECTORY)
        find_program(_PH_WINDEPLOYQT NAMES windeployqt PATHS "${_ph_qt_bin}" NO_DEFAULT_PATH)
        if(_PH_WINDEPLOYQT)
            add_custom_command(TARGET PowerHelper POST_BUILD
                COMMAND "${_PH_WINDEPLOYQT}" \"$<TARGET_FILE:PowerHelper>\"
                    --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                COMMENT "windeployqt PowerHelper..."
            )
        endif()
    endif()
endif()

# 渲染器手动测试 (EXCLUDE_FROM_ALL)
add_executable(ph_rendertest EXCLUDE_FROM_ALL
    test/render_test.cpp
)
target_link_libraries(ph_rendertest
    PRIVATE PowerHelperCore Qt6::Core
)
if(WIN32 AND NOT POWERHELPER_STANDALONE_ONLY)
    add_custom_command(TARGET ph_rendertest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:PowerHelperCore>" "$<TARGET_FILE_DIR:ph_rendertest>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libcmark-gfm>" "$<TARGET_FILE_DIR:ph_rendertest>/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:libcmark-gfm-extensions>" "$<TARGET_FILE_DIR:ph_rendertest>/"
        COMMENT "ph_rendertest: deploy core dlls"
    )
endif()
