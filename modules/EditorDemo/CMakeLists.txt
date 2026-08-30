cmake_minimum_required(VERSION 3.20)
add_executable(editor_demo
    src/main.cpp
)
target_include_directories(editor_demo
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../HiBerGUILibCPP/HiBerGUILibrary/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../HiBerGUILibCPP/HiBerGUIWebView2/include
)
target_link_libraries(editor_demo
    PRIVATE
        HiBerGUILibrary
        HiBerGUIWebView2
        Qt6::Core
        Qt6::Widgets
)

# 演示用部署: WebView2Loader.dll (体积对比: 含/不含 各约 200KB)
if(WIN32)
    if(CMAKE_BUILD_TYPE MATCHES "Debug")
        set(_wv2_loader "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/bin/WebView2Loader.dll")
    else()
        set(_wv2_loader "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/WebView2Loader.dll")
    endif()
    add_custom_command(TARGET editor_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_wv2_loader}" "$<TARGET_FILE_DIR:editor_demo>/"
        COMMENT "Deploy: WebView2Loader.dll -> editor_demo/"
    )
    get_target_property(_demo_qmake Qt6::qmake IMPORTED_LOCATION)
    if(_demo_qmake)
        get_filename_component(_demo_qt_bin "${_demo_qmake}" DIRECTORY)
        find_program(_DEMO_WINDEPLOYQT NAMES windeployqt PATHS "${_demo_qt_bin}" NO_DEFAULT_PATH)
        if(_DEMO_WINDEPLOYQT)
            add_custom_command(TARGET editor_demo POST_BUILD
                COMMAND "${_DEMO_WINDEPLOYQT}" \"$<TARGET_FILE:editor_demo>\"
                    --no-translations --no-opengl-sw --no-compiler-runtime --no-system-d3d-compiler
                COMMENT "windeployqt editor_demo..."
            )
        endif()
    endif()
endif()
