add_executable(nbtcpp_example
    example_main.cpp
)
target_link_libraries(nbtcpp_example PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(nbtcpp_region_example
    example_region.cpp
)
target_link_libraries(nbtcpp_region_example PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_region_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(nbtcpp_dump_mca
    dump_mca.cpp
)
target_link_libraries(nbtcpp_dump_mca PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_dump_mca PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(nbtcpp_dump_nbt
    dump_nbt.cpp
)
target_link_libraries(nbtcpp_dump_nbt PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_dump_nbt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(nbtcpp_roundtrip_test
    roundtrip_test.cpp
)
target_link_libraries(nbtcpp_roundtrip_test PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_roundtrip_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

add_executable(nbtcpp_diff_test
    diff_test.cpp
)
target_link_libraries(nbtcpp_diff_test PRIVATE nbtcpp ${ZLIB_TARGET})
target_include_directories(nbtcpp_diff_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
