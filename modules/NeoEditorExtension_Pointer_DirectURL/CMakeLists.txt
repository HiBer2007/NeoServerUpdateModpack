cmake_minimum_required(VERSION 3.20)

add_library(NeoEditorExtension_Pointer_DirectURL SHARED
    src/editor_directurl.cpp
    src/editor_directurl.h
)
target_include_directories(NeoEditorExtension_Pointer_DirectURL
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
)
target_link_libraries(NeoEditorExtension_Pointer_DirectURL
    PRIVATE NeoCore Qt6::Widgets
)
set_target_properties(NeoEditorExtension_Pointer_DirectURL PROPERTIES
    PREFIX ""
)
