# CLI 分发目录的 deploy 过滤复制：排除 crash-report/、*.dmp、config/custom/
# 用法: cmake -P release_cli_dist.cmake <SRC_DIR> <DST_DIR>

set(_src "${CMAKE_ARGV3}")
set(_dst "${CMAKE_ARGV4}")

file(REMOVE_RECURSE "${_dst}")
file(MAKE_DIRECTORY "${_dst}")

file(GLOB_RECURSE _files LIST_DIRECTORIES false RELATIVE "${_src}" "${_src}/*")
foreach(_rel IN LISTS _files)
    if(_rel MATCHES "^crash-report/" OR _rel MATCHES "/crash-report/"
       OR _rel MATCHES "\\.dmp$" OR _rel MATCHES "^config/custom/")
        continue()
    endif()
    get_filename_component(_dir "${_rel}" DIRECTORY)
    if(_dir)
        file(MAKE_DIRECTORY "${_dst}/${_dir}")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_src}/${_rel}" "${_dst}/${_rel}")
endforeach()

message(STATUS "CLI dist payload copied to ${_dst}")
