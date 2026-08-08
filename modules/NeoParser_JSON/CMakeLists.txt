add_library(NeoParser_JSON SHARED
    src/parser_json.cpp
)
target_include_directories(NeoParser_JSON
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoParser_JSON
    PRIVATE
        NeoCore
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)


