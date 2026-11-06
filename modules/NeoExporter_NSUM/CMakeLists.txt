add_library(NeoExporter_NSUM SHARED
    src/exporter_nsum.cpp
)
target_include_directories(NeoExporter_NSUM
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
    PRIVATE src
)
target_link_libraries(NeoExporter_NSUM
    PRIVATE
        NeoCore
        NeoBuild
        spdlog::spdlog
        Qt6::Core
        libzippp::libzippp
)
