cmake_minimum_required(VERSION 3.20)

# ============================================================
# GitIgnoreMarkup: 独立 .gitignore 标记/逆向标记系统 + 编辑 GUI 组件
#  - 纯逻辑部分 (gitignore_markup): 仅依赖 Qt Core, 不属 GUI 组件
#    (供 CLI/其他宿主复用); 命名空间 GitIgnoreMarkup
#  - 编辑 GUI 组件 (gitignore_dialog): 依赖 Qt Widgets + HiBerGUILibrary
#    (CodeEditor 编辑器套件), 命名空间 GitIgnoreMarkup
# ============================================================
add_library(GitIgnoreMarkup STATIC
    src/gitignore_markup.cpp
    src/gitignore_dialog.cpp
        include/gitignore_markup.h
        include/gitignore_dialog.h
)
target_include_directories(GitIgnoreMarkup
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(GitIgnoreMarkup
    PUBLIC
        Qt6::Core
    PRIVATE
        Qt6::Widgets
        HiBerGUILibrary
)