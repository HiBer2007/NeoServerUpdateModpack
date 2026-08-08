add_library(NeoCLI STATIC
    src/arg_parser.cpp
    src/cli_dispatcher.cpp
    src/cli_output.cpp
)
target_include_directories(NeoCLI
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoWorkspace/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
)
target_link_libraries(NeoCLI
    PUBLIC
        NeoCore
        NeoWorkspace
        NeoBuild
        Qt6::Core
        nlohmann_json::nlohmann_json
)





