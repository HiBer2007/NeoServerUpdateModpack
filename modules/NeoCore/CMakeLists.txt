add_library(NeoCore STATIC
    src/plugin_loader.cpp
    src/cancel_token.cpp
    src/git_analyzer.cpp
    src/error_codes.cpp
)
target_include_directories(NeoCore
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(NeoCore
    PUBLIC
        CommonLoggerCPP
        Qt6::Widgets
        nlohmann_json::nlohmann_json
)
