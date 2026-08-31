cmake_minimum_required(VERSION 3.20)

option(NSUM_ASAN "Enable AddressSanitizer for editor diagnostics" OFF)
if(NSUM_ASAN)
    foreach(t NeoCore NeoBuild NeoWorkspace GUIWorker HiBerGUILibrary CrashTrackerHandleLib)
        set_property(TARGET ${t} PROPERTY COMPILE_OPTIONS /fsanitize=address)
    endforeach()
endif()

# 控制台子系统 (与主程序同款): GUI 模式经 holdOrReleaseConsole 保持/释放终端,
# 便于从 cmd 启动时输出日志到终端调试
add_executable(NeoWorkspaceEditor
    src/main.cpp
    src/editor_tui.cpp
    src/editor_tui.h
    src/editor_window.cpp
    src/editor_window.h
    src/branch_meta_dialog.cpp
    src/branch_meta_dialog.h
    src/app.rc
)
target_include_directories(NeoWorkspaceEditor
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../GUIWorker/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../HiBerGUILibCPP/HiBerGUILibrary/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../PowerHelper/bridge/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoWorkspace/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
)
target_link_libraries(NeoWorkspaceEditor
    PRIVATE
        GitIgnoreMarkup
        NeoCore
        NeoWorkspace
        NeoBuild
        GUIWorker
        HiBerGUILibrary
        PowerHelperBridgeLib
        CrashTrackerHandleLib
        Qt6::Core
        Qt6::Widgets
        libzippp::libzippp
        ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/zip.lib
)

nsum_add_version_info(NeoWorkspaceEditor
    "NSUM 仓库管理器 - 工作区配置编辑器"
    "NSUM构建工具")

if(NSUM_ASAN)
    target_compile_options(NeoWorkspaceEditor PRIVATE /fsanitize=address)
    target_link_options(NeoWorkspaceEditor PRIVATE /fsanitize=address)
endif()

# windeployqt
if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake)
        get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
        find_program(_WINDEPLOYQT NAMES windeployqt PATHS "${_qt_bin_dir}" NO_DEFAULT_PATH)
        if(_WINDEPLOYQT)
            add_custom_command(TARGET NeoWorkspaceEditor POST_BUILD
                COMMAND "${_WINDEPLOYQT}" \"$<TARGET_FILE:NeoWorkspaceEditor>\"
                    --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                COMMENT "windeployqt NeoWorkspaceEditor..."
            )
        endif()
    endif()
endif()
