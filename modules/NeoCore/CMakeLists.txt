add_library(NeoCore STATIC
    src/plugin_loader.cpp
    src/cancel_token.cpp
    src/git_analyzer.cpp
    src/error_codes.cpp
    src/parallel_downloader.cpp
)
target_include_directories(NeoCore
    PUBLIC  include
    PRIVATE src
)
target_link_libraries(NeoCore
    PUBLIC
        CommonLoggerCPP
        PowerHelperBridgeLib
        Qt6::Widgets
        Qt6::Network
        nlohmann_json::nlohmann_json
)
