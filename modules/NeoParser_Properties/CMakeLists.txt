add_library(NeoParser_Properties SHARED
    src/parser_properties.cpp
)
target_include_directories(NeoParser_Properties
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoParser_Properties
    PRIVATE
        NeoCore
        spdlog::spdlog
)
