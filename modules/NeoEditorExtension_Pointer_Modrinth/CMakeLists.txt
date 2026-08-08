cmake_minimum_required(VERSION 3.20)

add_library(NeoEditorExtension_Pointer_Modrinth SHARED
    src/editor_modrinth.cpp
    src/editor_modrinth.h
)
target_include_directories(NeoEditorExtension_Pointer_Modrinth
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
)
target_link_libraries(NeoEditorExtension_Pointer_Modrinth
    PRIVATE NeoCore Qt6::Widgets Qt6::Network
)
set_target_properties(NeoEditorExtension_Pointer_Modrinth PROPERTIES
    PREFIX ""
)
