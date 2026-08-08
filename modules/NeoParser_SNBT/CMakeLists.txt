add_library(NeoParser_SNBT SHARED
    src/parser_snbt.cpp
    src/snbt_preserving_writer.cpp
)
target_include_directories(NeoParser_SNBT
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../nbtcpp/include
    PRIVATE src
)
target_link_libraries(NeoParser_SNBT
    PRIVATE
        NeoCore
        spdlog::spdlog
        nbtcpp
)


