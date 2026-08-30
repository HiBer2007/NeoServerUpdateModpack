add_library(NeoBuild STATIC
    src/build_engine.cpp
    src/branch_merger.cpp
    src/modpack_exporter.cpp
    src/serverconfig_sync.cpp
    src/pointer_downloader.cpp
    src/umd_generator.cpp
    src/sync_policy_executor.cpp
    src/mod_metadata.cpp
    src/platform_api.cpp
)
target_include_directories(NeoBuild
    PUBLIC  include
    PRIVATE src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoWorkspace/include
)
target_link_libraries(NeoBuild
    PUBLIC
        NeoCore
        NeoWorkspace
        Qt6::Core
        Qt6::Network
        libzippp::libzippp
    PRIVATE
        tomlplusplus::tomlplusplus
)


