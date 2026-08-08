add_library(NeoParser_TOML SHARED
    src/parser_toml.cpp
)
target_include_directories(NeoParser_TOML
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoParser_TOML
    PRIVATE
        NeoCore
        spdlog::spdlog
        tomlplusplus::tomlplusplus
)


