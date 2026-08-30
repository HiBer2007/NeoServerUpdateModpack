add_library(NeoWorkspace STATIC
    src/workspace_manager.cpp
    src/git_operations.cpp
    src/sync_engine.cpp
    src/file_scanner.cpp
    src/history_store.cpp
)
target_include_directories(NeoWorkspace
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
)
target_link_libraries(NeoWorkspace
    PUBLIC
        NeoCore
        Qt6::Core
        nlohmann_json::nlohmann_json
)


