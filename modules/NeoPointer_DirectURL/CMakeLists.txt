add_library(NeoPointer_DirectURL SHARED
    src/pointer_directurl.cpp
)
target_include_directories(NeoPointer_DirectURL
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoPointer_DirectURL
    PRIVATE
        NeoCore
        spdlog::spdlog
        Qt6::Core
        Qt6::Network
)


