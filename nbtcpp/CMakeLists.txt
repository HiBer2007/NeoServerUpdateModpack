cmake_minimum_required(VERSION 3.14)
project(nbtcpp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ─── Options ──────────────────────────────────────────────────────────────────
option(NBTCPP_BUILD_EXAMPLES "Build example programs" ON)
option(NBTCPP_BUILD_TESTS    "Build unit tests"       OFF)

# ─── Local zlib ───────────────────────────────────────────────────────────────
# zlib source is bundled at lib/zlib/ — no network fetch needed.
set(SKIP_INSTALL_LIBRARIES ON)
set(SKIP_BUILD_TEST       ON)
set(ZLIB_BUILD_EXAMPLES   OFF)

add_subdirectory(lib/zlib EXCLUDE_FROM_ALL)

if(TARGET zlibstatic)
    set(ZLIB_TARGET zlibstatic)
else()
    set(ZLIB_TARGET zlib)
endif()
message(STATUS "ZLIB: using bundled ${ZLIB_TARGET} from lib/zlib/")

# ─── Library sources ───────────────────────────────────────────────────────────
set(NBTCPP_SOURCES
    src/nbt_binary_reader.cpp
    src/nbt_binary_writer.cpp
    src/nbt_reader.cpp
    src/nbt_writer.cpp
    src/nbt_file.cpp
    src/tags/nbt_tag.cpp
    src/tags/nbt_container_tag.cpp
    src/tags/nbt_byte.cpp
    src/tags/nbt_short.cpp
    src/tags/nbt_int.cpp
    src/tags/nbt_long.cpp
    src/tags/nbt_float.cpp
    src/tags/nbt_double.cpp
    src/tags/nbt_string.cpp
    src/tags/nbt_byte_array.cpp
    src/tags/nbt_int_array.cpp
    src/tags/nbt_long_array.cpp
    src/tags/nbt_list.cpp
    src/tags/nbt_compound.cpp
    src/region_file.cpp
    src/chunk.cpp
    src/snbt/snbt_parser.cpp
    src/snbt/snbt_maker.cpp
    src/nbt_diff.cpp
)

# ─── Library ───────────────────────────────────────────────────────────────────
add_library(nbtcpp STATIC ${NBTCPP_SOURCES})
target_include_directories(nbtcpp
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    PRIVATE src
)
target_link_libraries(nbtcpp PRIVATE ${ZLIB_TARGET})
target_compile_definitions(nbtcpp PRIVATE NBTCPP_HAS_ZLIB)

# ─── Examples ──────────────────────────────────────────────────────────────────
if(NBTCPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ─── Tests ─────────────────────────────────────────────────────────────────────
if(NBTCPP_BUILD_TESTS)
    enable_testing()
    # Add test subdirectory here when tests are implemented
endif()

# ─── Install ───────────────────────────────────────────────────────────────────
# Only try to export when installing properly (skipped during dev builds)
# to avoid zlib's build-directory include paths causing errors.
# For distribution, configure with -DNBTCPP_INSTALL=ON and use system zlib.
option(NBTCPP_INSTALL "Generate install targets" OFF)
if(NBTCPP_INSTALL)
    install(TARGETS nbtcpp ${ZLIB_TARGET} EXPORT nbtcpp-config
        ARCHIVE  DESTINATION lib
        LIBRARY  DESTINATION lib
        RUNTIME  DESTINATION bin
    )
    install(DIRECTORY include/ DESTINATION include)
    install(EXPORT nbtcpp-config DESTINATION share/nbtcpp/cmake)
endif()
