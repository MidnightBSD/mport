cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.list)

set(mport.list_source_files mport.list.c)
add_executable(mport.list ${mport.list_source_files})

target_link_libraries(mport.list libmport)