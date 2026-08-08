add_library(NeoParser_YAML SHARED
    src/parser_yaml.cpp
)
target_include_directories(NeoParser_YAML
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoParser_YAML
    PRIVATE
        NeoCore
        spdlog::spdlog
        yaml-cpp::yaml-cpp
)


