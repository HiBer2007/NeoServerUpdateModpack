cmake_minimum_required(VERSION 3.20)
project(NeoServerUpdateModpack VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(WIN32 AND NOT DEFINED CMAKE_RC_COMPILER)
    find_program(WIN_RC_COMPILER NAMES rc
        PATHS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
              "C:/Program Files (x86)/Windows Kits/10/bin/10.0.22621.0/x64"
        NO_DEFAULT_PATH
    )
    if(WIN_RC_COMPILER)
        set(CMAKE_RC_COMPILER "${WIN_RC_COMPILER}" CACHE FILEPATH "Resource Compiler")
    endif()
endif()

# ============================================================
# 版本资源: 为 EXE/DLL 写入 VERSIONINFO (描述、版本、产品名等)
# 用法: nsum_add_version_info(<target> "<FileDescription>" "<ProductName>")
# ============================================================
set(NSUM_VERSION_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/version.rc.in")

function(nsum_add_version_info target desc product)
    if(NOT TARGET ${target})
        return()
    endif()
    get_target_property(_nsum_type ${target} TYPE)
    if(_nsum_type STREQUAL "EXECUTABLE")
        set(_nsum_filetype "VFT_APP")
        set(_nsum_original "${target}.exe")
    else()
        set(_nsum_filetype "VFT_DLL")
        set(_nsum_original "${target}.dll")
    endif()

    set(_nsum_icon_block "")
    if(ARGC GREATER 3)
        set(_nsum_icon "${ARGV3}")
        if(EXISTS "${_nsum_icon}")
            set(_nsum_icon_block "IDI_ICON1 ICON \"${_nsum_icon}\"")
        endif()
    endif()

    set(_nsum_rc "${CMAKE_CURRENT_BINARY_DIR}/version_${target}.rc")
    file(READ "${NSUM_VERSION_TEMPLATE}" _nsum_content)
    string(REPLACE "@NSUM_VER_MAJOR@" "${PROJECT_VERSION_MAJOR}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_VER_MINOR@" "${PROJECT_VERSION_MINOR}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_VER_PATCH@" "${PROJECT_VERSION_PATCH}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_VERSION@" "${PROJECT_VERSION}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_DESC@" "${desc}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_PRODUCT@" "${product}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_INTERNAL@" "${target}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_ORIGINAL@" "${_nsum_original}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_FILETYPE@" "${_nsum_filetype}" _nsum_content "${_nsum_content}")
    string(REPLACE "@NSUM_ICON_BLOCK@" "${_nsum_icon_block}" _nsum_content "${_nsum_content}")
    set(_nsum_utf8bom "")
    string(ASCII 239 187 191 _nsum_utf8bom)
    file(WRITE "${_nsum_rc}" "${_nsum_utf8bom}${_nsum_content}")
    target_sources(${target} PRIVATE "${_nsum_rc}")
endfunction()

set(NBTCPP_BUILD_EXAMPLES OFF)
add_subdirectory(nbtcpp EXCLUDE_FROM_ALL)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Core)

if(NOT DEFINED INSTALLER_ONLY_BUILD)
    find_package(nlohmann_json REQUIRED)
    find_package(yaml-cpp REQUIRED)
    find_package(tomlplusplus REQUIRED)
    find_package(spdlog REQUIRED)
    find_package(libzippp REQUIRED)
endif()

# NeoInstaller — 仅通过 installer-static 预设独立构建，不参与主配置
if(INSTALLER_ONLY_BUILD)
    add_subdirectory(modules/NeoInstaller)
    nsum_add_version_info(NeoInstaller
        "NSUM 构建工具安装程序"
        "NSUM构建工具")
endif()

if(NOT DEFINED INSTALLER_ONLY_BUILD)
# ============================================================
# Shared Libraries (引擎核心，静态库)
# ============================================================
add_subdirectory(modules/CommonLoggerCPP)
add_subdirectory(modules/HiBerGUILibCPP)
add_subdirectory(modules/GitIgnoreMarkup)
add_subdirectory(modules/NeoCore)
add_subdirectory(modules/NeoWorkspace)
add_subdirectory(modules/NeoBuild)

# ============================================================
# Plugin DLLs — Config Parsers (运行时加载)
# ============================================================
add_subdirectory(modules/NeoParser_JSON)
add_subdirectory(modules/NeoParser_YAML)
add_subdirectory(modules/NeoParser_TOML)
add_subdirectory(modules/NeoParser_SNBT)
add_subdirectory(modules/NeoParser_TXT)
add_subdirectory(modules/NeoParser_Properties)

# ============================================================
# Plugin DLLs — Pointer Resolvers (运行时加载)
# ============================================================
add_subdirectory(modules/NeoPointer_Modrinth)
add_subdirectory(modules/NeoPointer_DirectURL)

# ============================================================
# Plugin DLLs — Exporters (运行时加载)
# ============================================================
add_subdirectory(modules/NeoExporter_MCBBS)
add_subdirectory(modules/NeoExporter_Modrinth)
add_subdirectory(modules/NeoExporter_HMCL)

# ============================================================
# Editor Extensions DLLs (编辑器扩展，运行时加载)
# ============================================================
add_subdirectory(modules/NeoEditorExtension_Pointer_Modrinth)
add_subdirectory(modules/NeoEditorExtension_Pointer_DirectURL)
add_subdirectory(modules/NeoEditorExtension_Parser_JSON)
add_subdirectory(modules/NeoEditorExtension_Parser_YAML)
add_subdirectory(modules/NeoEditorExtension_Parser_TOML)
add_subdirectory(modules/NeoEditorExtension_Parser_SNBT)
add_subdirectory(modules/NeoEditorExtension_Parser_TXT)
add_subdirectory(modules/NeoEditorExtension_Parser_Properties)

# ============================================================
# GUI / CLI (静态链接共享库)
# ============================================================
add_subdirectory(modules/EditorDemo)
add_subdirectory(modules/GUIWorker)
add_subdirectory(modules/NeoCLI)
add_subdirectory(modules/PowerHelper)

# ============================================================
# CrashTracker (崩溃捕获 + 分析工具)
# ============================================================
add_subdirectory(modules/CrashTrackerHandleLib)
add_subdirectory(modules/CrashTracker)

# ============================================================
# Executables
# ============================================================
add_subdirectory(modules/NeoWorkspaceEditor)
endif()

# ============================================================
# 主程序 (仅非 Installer 构建)
# ============================================================
if(NOT DEFINED INSTALLER_ONLY_BUILD)
file(GLOB_RECURSE APP_SOURCES src/main.cpp resources/app.rc)
add_executable(${PROJECT_NAME} ${APP_SOURCES})
target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME}
    PRIVATE NeoCore NeoWorkspace NeoBuild NeoCLI GUIWorker HiBerGUILibrary CrashTrackerHandleLib
    PRIVATE libzippp::libzippp
    PRIVATE ${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/zip.lib
)

nsum_add_version_info(${PROJECT_NAME}
    "NSUM 构建工具 - 新服务器自动更新整合包控制器"
    "NSUM构建工具")

# ============================================================
# 插件目标列表
# ============================================================
set(PARSER_TARGETS
    NeoParser_JSON NeoParser_YAML NeoParser_TOML NeoParser_SNBT NeoParser_TXT
    NeoParser_Properties)
set(POINTER_TARGETS
    NeoPointer_Modrinth NeoPointer_DirectURL)
set(EXPORTER_TARGETS
    NeoExporter_MCBBS NeoExporter_Modrinth NeoExporter_HMCL)

set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

# ============================================================
# 部署目标: 构建时始终执行，仅在依赖变更时复制文件
# ============================================================
set(DEPLOY_DEPS ${PROJECT_NAME} NeoWorkspaceEditor CrashTracker PowerHelper
    ${PARSER_TARGETS} ${POINTER_TARGETS} ${EXPORTER_TARGETS})

add_custom_target(neo_deploy ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/parsers"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/pointers"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/exporters"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>" "${DEPLOY_DIR}/"
    COMMENT "Deploy: preparing deploy directory"
    DEPENDS ${DEPLOY_DEPS}
)

if(TARGET NeoWorkspaceEditor)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:NeoWorkspaceEditor>" "${DEPLOY_DIR}/"
        COMMENT "Deploy: NeoWorkspaceEditor.exe -> deploy/"
    )
endif()

if(TARGET CrashTracker)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:CrashTracker>" "${DEPLOY_DIR}/"
        COMMENT "Deploy: CrashTracker.exe -> deploy/"
    )
endif()

if(TARGET PowerHelper AND NOT POWERHELPER_STANDALONE_ONLY)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:PowerHelper>" "${DEPLOY_DIR}/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:PowerHelperCore>" "${DEPLOY_DIR}/"
        COMMENT "Deploy: PowerHelper.exe + PowerHelperCore.dll -> deploy/"
    )
endif()

add_custom_command(TARGET neo_deploy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/deploy" "${DEPLOY_DIR}/docs"
    COMMENT "Deploy: docs/deploy/ -> deploy/docs/"
)

# windeployqt: 对 deploy 目录中的 EXE 执行 (Qt DLL 直接进入 deploy/)
if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake)
        get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
        find_program(WINDEPLOYQT NAMES windeployqt PATHS "${_qt_bin_dir}" NO_DEFAULT_PATH)
        if(WINDEPLOYQT)
            add_custom_command(TARGET neo_deploy POST_BUILD
                COMMAND "${WINDEPLOYQT}"
                    \"${DEPLOY_DIR}/${PROJECT_NAME}.exe\"
                    --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                COMMENT "windeployqt -> deploy/"
            )
            if(TARGET NeoWorkspaceEditor)
                add_custom_command(TARGET neo_deploy POST_BUILD
                    COMMAND "${WINDEPLOYQT}"
                        \"${DEPLOY_DIR}/NeoWorkspaceEditor.exe\"
                        --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                    COMMENT "windeployqt NeoWorkspaceEditor -> deploy/"
                )
            endif()
            if(TARGET PowerHelper AND NOT POWERHELPER_STANDALONE_ONLY)
                add_custom_command(TARGET neo_deploy POST_BUILD
                    COMMAND "${WINDEPLOYQT}"
                        \"${DEPLOY_DIR}/PowerHelper.exe\"
                        --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                    COMMENT "windeployqt PowerHelper -> deploy/"
                )
            endif()
        endif()
    endif()
endif()

foreach(parser IN LISTS PARSER_TARGETS)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${parser}>" "${DEPLOY_DIR}/parsers/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/${parser}/${parser}.meta.json"
            "${DEPLOY_DIR}/parsers/"
        COMMENT "Deploy: ${parser} -> deploy/parsers/"
    )
endforeach()

foreach(ptr IN LISTS POINTER_TARGETS)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${ptr}>" "${DEPLOY_DIR}/pointers/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/${ptr}/${ptr}.meta.json"
            "${DEPLOY_DIR}/pointers/"
        COMMENT "Deploy: ${ptr} -> deploy/pointers/"
    )
endforeach()

foreach(exporter IN LISTS EXPORTER_TARGETS)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${exporter}>" "${DEPLOY_DIR}/exporters/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/${exporter}/${exporter}.meta.json"
            "${DEPLOY_DIR}/exporters/"
        COMMENT "Deploy: ${exporter} -> deploy/exporters/"
    )
endforeach()

set(EDITOR_EXT_TARGETS
    NeoEditorExtension_Pointer_Modrinth
    NeoEditorExtension_Pointer_DirectURL)

add_custom_command(TARGET neo_deploy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/editor/extension/pointer"
    COMMENT "Deploy: editor/extension/pointer/"
)
foreach(ext IN LISTS EDITOR_EXT_TARGETS)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${ext}>" "${DEPLOY_DIR}/editor/extension/pointer/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/${ext}/${ext}.meta.json"
            "${DEPLOY_DIR}/editor/extension/pointer/"
        COMMENT "Deploy: ${ext} -> deploy/editor/extension/pointer/"
    )
endforeach()

set(PARSER_EDITOR_TARGETS
    NeoEditorExtension_Parser_JSON NeoEditorExtension_Parser_YAML
    NeoEditorExtension_Parser_TOML NeoEditorExtension_Parser_SNBT
    NeoEditorExtension_Parser_TXT NeoEditorExtension_Parser_Properties)

add_custom_command(TARGET neo_deploy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}/editor/extension/parsers"
    COMMENT "Deploy: editor/extension/parsers/"
)
foreach(ext IN LISTS PARSER_EDITOR_TARGETS)
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:${ext}>" "${DEPLOY_DIR}/editor/extension/parsers/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/modules/${ext}/${ext}.meta.json"
            "${DEPLOY_DIR}/editor/extension/parsers/"
        COMMENT "Deploy: ${ext} -> deploy/editor/extension/parsers/"
    )
endforeach()

# ============================================================
# 拷贝 vcpkg 运行时 DLL 到 deploy/ (构建时执行)
# ============================================================
if(WIN32)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(VCPKG_RUNTIME_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin")
    else()
        set(VCPKG_RUNTIME_DIR "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin")
    endif()
    add_custom_command(TARGET neo_deploy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -D SRC="${VCPKG_RUNTIME_DIR}" -D DST="${DEPLOY_DIR}" -P
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime_dlls.cmake"
        COMMENT "Deploy: vcpkg runtime DLLs -> deploy/"
    )
endif()

# ============================================================
# 版本资源: 插件 DLL (VERSIONINFO)
# ============================================================
foreach(_t NeoParser_JSON NeoParser_YAML NeoParser_TOML NeoParser_SNBT NeoParser_TXT NeoParser_Properties)
    nsum_add_version_info(${_t} "NSUM 配置解析器插件 (${_t})" "NSUM构建工具")
endforeach()
foreach(_t NeoPointer_Modrinth NeoPointer_DirectURL)
    nsum_add_version_info(${_t} "NSUM 指针解析器插件 (${_t})" "NSUM构建工具")
endforeach()
foreach(_t NeoExporter_MCBBS NeoExporter_Modrinth NeoExporter_HMCL)
    nsum_add_version_info(${_t} "NSUM 导出插件 (${_t})" "NSUM构建工具")
endforeach()
foreach(_t NeoEditorExtension_Pointer_Modrinth NeoEditorExtension_Pointer_DirectURL)
    nsum_add_version_info(${_t} "NSUM 指针编辑器扩展 (${_t})" "NSUM构建工具")
endforeach()
foreach(_t NeoEditorExtension_Parser_JSON NeoEditorExtension_Parser_YAML
       NeoEditorExtension_Parser_TOML NeoEditorExtension_Parser_SNBT
       NeoEditorExtension_Parser_TXT NeoEditorExtension_Parser_Properties)
    nsum_add_version_info(${_t} "NSUM 配置编辑器扩展 (${_t})" "NSUM构建工具")
endforeach()

endif() # INSTALLER_ONLY_BUILD
