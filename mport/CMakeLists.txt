cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport_exe)

set(mport_exe_source_files mport.c)
add_executable(mport_exe ${mport_exe_source_files})

target_link_libraries(mport_exe libmport)