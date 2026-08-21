cmake_minimum_required(VERSION 3.20)
find_package(unofficial-webview2 CONFIG REQUIRED)

# Web 版代码编辑器 (STATIC): 仅链接本库的程序才依赖 WebView2 (WebView2Loader.dll)
add_library(HiBerGUIWebEditor STATIC
    src/web_code_editor.cpp
        include/web_code_editor.h
)
target_include_directories(HiBerGUIWebEditor
    PUBLIC  include
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../HiBerGUILibrary/include
    PRIVATE src
)
target_link_libraries(HiBerGUIWebEditor
    PUBLIC
        Qt6::Core
        Qt6::Widgets
    PRIVATE
        HiBerGUILibrary
        unofficial::webview2::webview2
)
