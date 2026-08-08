add_library(NeoExporter_HMCL SHARED
    src/exporter_hmcl.cpp
)
target_include_directories(NeoExporter_HMCL
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
    PRIVATE src
)
target_link_libraries(NeoExporter_HMCL
    PRIVATE
        NeoCore
        NeoBuild
        spdlog::spdlog
        Qt6::Core
        libzippp::libzippp
)
