add_library(GUIWorker STATIC
    src/wizard_window.cpp
    src/repo_page.cpp
    src/branch_page.cpp
    src/modpack_page.cpp
    src/export_type_page.cpp
    src/export_dir_page.cpp
    src/build_page.cpp
    src/build_tool_window.cpp
    src/pointer_editor.cpp
    src/pointer_manager.cpp
    src/build_checklist_page.cpp
    src/extra_info_page.cpp
    src/done_page.cpp
    src/repo_editor.cpp
    src/branch_editor.cpp
    src/modpack_content_ide.cpp
    src/folder_policy_editor.cpp
    src/config_file_editor.cpp
    src/pointer_editor_panel.cpp
    src/serverconfig_rules_editor.cpp
    src/sync_policies_editor.cpp
        include/build_checklist_page.h
        include/extra_info_page.h
        include/done_page.h
        include/branch_editor.h
        include/branch_page.h
        include/build_page.h
        include/build_tool_window.h
        include/export_dir_page.h
        include/export_type_page.h
        include/folder_policy_editor.h
        include/config_file_editor.h
        include/modpack_page.h
        include/pointer_editor.h
        include/pointer_manager.h
        include/repo_editor.h
        include/repo_page.h
        include/wizard_window.h
        include/modpack_content_ide.h
        include/pointer_editor_panel.h
        include/serverconfig_rules_editor.h
        include/sync_policies_editor.h
        include/sync_policy_display.h
)
target_include_directories(GUIWorker
    PUBLIC  include
    PRIVATE src
    PRIVATE /../NeoCore/include
    PRIVATE /../NeoBuild/include
    PRIVATE /../NeoWorkspace/include
)
target_link_libraries(GUIWorker
    PUBLIC
        HiBerGUILibrary
        NeoCore
        NeoBuild
        Qt6::Core
        Qt6::Widgets
    PRIVATE
        PowerHelperBridge
        libzippp::libzippp
)
