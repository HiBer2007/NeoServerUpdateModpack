# NeoInstaller — 使用 vcpkg 工具链 (同主构建)
cmake_minimum_required(VERSION 3.20)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 部署文件源 (通过预设 -DDEPLOY_SOURCE=... 传入)
if(NOT DEFINED DEPLOY_SOURCE)
    set(DEPLOY_SOURCE "${CMAKE_SOURCE_DIR}/build/deploy")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# 检测静态 Qt
set(IS_STATIC_QT OFF)
if(VCPKG_TARGET_TRIPLET MATCHES "static" OR MINGW)
    set(IS_STATIC_QT ON)
endif()

# 生成 installer.qrc — 嵌入部署文件 + LICENSE
set(QRC_OUT "${CMAKE_CURRENT_BINARY_DIR}/installer.qrc")
file(WRITE ${QRC_OUT} "<RCC>\n  <qresource prefix=\"/deploy/\">\n")

if(EXISTS "${DEPLOY_SOURCE}")
    file(GLOB_RECURSE EMBED_FILES LIST_DIRECTORIES false "${DEPLOY_SOURCE}/*")
    set(FILE_COUNT 0)
    foreach(f IN LISTS EMBED_FILES)
        get_filename_component(fext "${f}" LAST_EXT)
        if(fext STREQUAL ".dmp")
            continue()
        endif()
        file(RELATIVE_PATH rel "${DEPLOY_SOURCE}" "${f}")
        if(rel MATCHES "^config/custom/")
            continue()
        endif()
        string(REPLACE "\\" "/" rel "${rel}")
        file(APPEND ${QRC_OUT} "    <file alias=\"${rel}\">${f}</file>\n")
        math(EXPR FILE_COUNT "${FILE_COUNT} + 1")
    endforeach()
    message(STATUS "NeoInstaller: embedding ${FILE_COUNT} files from ${DEPLOY_SOURCE}")
    set(HAS_DEPLOY_FILES TRUE)
else()
    message(FATAL_ERROR "Deploy dir not found: ${DEPLOY_SOURCE} — build main project first (msvc preset)")
endif()
file(APPEND ${QRC_OUT} "  </qresource>\n")
file(APPEND ${QRC_OUT} "  <qresource prefix=\"/license/\">\n")
file(APPEND ${QRC_OUT} "    <file alias=\"LICENSE\">${CMAKE_SOURCE_DIR}/LICENSE</file>\n")
file(APPEND ${QRC_OUT} "  </qresource>\n")
file(APPEND ${QRC_OUT} "  <qresource prefix=\"/tools/\">\n")
file(APPEND ${QRC_OUT} "    <file alias=\"7za.exe\">${CMAKE_CURRENT_SOURCE_DIR}/tools/7za.exe</file>\n")
file(APPEND ${QRC_OUT} "  </qresource>\n")
file(APPEND ${QRC_OUT} "</RCC>\n")

set(INSTALLER_SOURCES
    src/installer_main.cpp
    src/installer_wizard.cpp
    src/git_checker.cpp
    src/silent_installer.cpp
    src/git_downloader.cpp
    src/app.rc
    ${QRC_OUT}
)

qt_add_executable(NeoInstaller WIN32 ${INSTALLER_SOURCES})
target_include_directories(NeoInstaller PRIVATE src)
target_link_libraries(NeoInstaller PRIVATE Qt6::Core Qt6::Widgets Qt6::Network)

# 静态模式特殊处理
if(IS_STATIC_QT)
    if(MSVC)
        qt_import_plugins(NeoInstaller INCLUDE Qt::QWindowsIntegrationPlugin)
        message(STATUS "NeoInstaller: QWindowsIntegrationPlugin imported")
    endif()
    if(MINGW)
        target_link_libraries(NeoInstaller PRIVATE
            -static -static-libgcc -static-libstdc++
            -Wl,-Bstatic -lstdc++ -lpthread)
        target_link_options(NeoInstaller PRIVATE -Wl,--gc-sections -Wl,-s)
    endif()
endif()

# 动态模式: windeployqt
if(NOT IS_STATIC_QT AND WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_qt_qmake)
        get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
        find_program(_WD NAMES windeployqt PATHS "${_qt_bin}" NO_DEFAULT_PATH)
        if(_WD)
            add_custom_command(TARGET NeoInstaller POST_BUILD
                COMMAND "${_WD}" \"$<TARGET_FILE:NeoInstaller>\"
                    --no-translations --no-opengl-sw --no-compiler-runtime
                COMMENT "windeployqt NeoInstaller (dynamic)..."
            )
        endif()
    endif()
endif()
