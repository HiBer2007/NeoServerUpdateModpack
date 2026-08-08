add_library(NeoParser_TXT SHARED
    src/parser_txt.cpp
)
target_include_directories(NeoParser_TXT
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoParser_TXT
    PRIVATE
        NeoCore
        spdlog::spdlog
)


