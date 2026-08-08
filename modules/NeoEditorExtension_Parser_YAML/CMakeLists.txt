cmake_minimum_required(VERSION 3.20)
add_library(NeoEditorExtension_Parser_YAML SHARED
    ../NeoEditorExtension_Parser_JSON/src/config_editor.h
    ../NeoEditorExtension_Parser_JSON/src/config_editor.cpp
    src/editor_parser_yaml.cpp)
target_include_directories(NeoEditorExtension_Parser_YAML PRIVATE src PRIVATE ../NeoEditorExtension_Parser_JSON/src PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../NeoCore/include)
target_link_libraries(NeoEditorExtension_Parser_YAML PRIVATE NeoCore Qt6::Widgets)
set_target_properties(NeoEditorExtension_Parser_YAML PROPERTIES PREFIX "")
