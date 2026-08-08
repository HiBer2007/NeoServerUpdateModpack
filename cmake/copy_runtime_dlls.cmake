# Build-time script: copy vcpkg runtime DLLs to deploy directory
# Called from add_custom_command (no configure-time file(GLOB))

if(NOT EXISTS "${SRC}")
    message(STATUS "Deploy: runtime DLL source dir not found — skipping: ${SRC}")
    return()
endif()

file(GLOB DLLS "${SRC}/*.dll")
foreach(dll IN LISTS DLLS)
    get_filename_component(name "${dll}" NAME)
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "${DST}/${name}"
        RESULT_VARIABLE result
    )
endforeach()
