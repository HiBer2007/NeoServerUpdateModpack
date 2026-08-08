add_library(NeoExporter_Modrinth SHARED
    src/exporter_modrinth.cpp
)
target_include_directories(NeoExporter_Modrinth
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
    PRIVATE src
)
target_link_libraries(NeoExporter_Modrinth
    PRIVATE
        NeoCore
        NeoBuild
        spdlog::spdlog
        Qt6::Core
        libzippp::libzippp
)
