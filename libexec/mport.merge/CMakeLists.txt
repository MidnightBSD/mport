cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.merge)

set(mport.merge_source_files mport.merge.c)
add_executable(mport.merge ${mport.merge_source_files})

target_link_libraries(mport.merge libmport)