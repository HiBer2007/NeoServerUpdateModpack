# hci_gui 便捷释放：把可执行程序及其运行时 DLL 复制到指定根目录。
# 用法: cmake -P release_hci_gui.cmake <SRC_DIR> <DST_DIR>
# 注意: -P 模式下 CMAKE_ARGV 为整条命令行 argv（ARGV0=cmake 自身, ARGV1=-P,
#       ARGV2=脚本路径）——用户参数从 ARGV3 开始。
# 复制 SRC 中全部 *.exe 与 *.dll（过滤 .rc/.qrc/.manifest 等中间产物）；
# 静态构建（无 DLL）时仅复制 exe。

set(_src "${CMAKE_ARGV3}")
set(_dst "${CMAKE_ARGV4}")

file(GLOB _exes "${_src}/*.exe")
file(GLOB _dlls "${_src}/*.dll")
set(_files ${_exes} ${_dlls})

foreach(_f IN LISTS _files)
    if(EXISTS "${_f}")
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_f}" "${_dst}")
    endif()
endforeach()

message(STATUS "Released installer EXE (and runtime DLLs if any) to ${_dst}")