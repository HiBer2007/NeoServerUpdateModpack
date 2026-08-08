add_library(NeoExporter_MCBBS SHARED
    src/exporter_mcbbs.cpp
)
target_include_directories(NeoExporter_MCBBS
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoBuild/include
    PRIVATE src
)
target_link_libraries(NeoExporter_MCBBS
    PRIVATE
        NeoCore
        NeoBuild
        spdlog::spdlog
        Qt6::Core
        libzippp::libzippp
)
