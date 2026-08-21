add_library(HiBerGUILibrary STATIC
    src/animated_progress.cpp
    src/toast_notification.cpp
    src/progress_card.cpp
    src/work_card.cpp
    src/work_card_stack.cpp
    src/code_editor.cpp
    src/merge_preview_dialog.cpp
    src/deep_tree_behavior.cpp
    src/output_tree_panel.cpp
    src/repo_tree_panel.cpp
    src/file_content_editor.cpp
    src/git_panel.cpp
        include/animated_progress.h
        include/toast_notification.h
        include/progress_card.h
        include/work_card.h
        include/work_card_stack.h
        include/code_editor_interface.h
        include/code_editor.h
        include/merge_preview_dialog.h
        include/deep_tree_behavior.h
        include/output_tree_panel.h
        include/repo_tree_panel.h
        include/file_content_editor.h
        include/git_panel.h
)
target_include_directories(HiBerGUILibrary
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(HiBerGUILibrary
    PUBLIC
        Qt6::Core
        Qt6::Widgets
        nlohmann_json::nlohmann_json
)
