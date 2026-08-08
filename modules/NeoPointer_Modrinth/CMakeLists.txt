add_library(NeoPointer_Modrinth SHARED
    src/pointer_modrinth.cpp
)
target_include_directories(NeoPointer_Modrinth
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../NeoCore/include
    PRIVATE src
)
target_link_libraries(NeoPointer_Modrinth
    PRIVATE
        NeoCore
        spdlog::spdlog
        Qt6::Core
        Qt6::Network
)


