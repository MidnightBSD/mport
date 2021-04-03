cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.version_cmp)

set(mport.version_cmp_source_files mport.version_cmp.c)
add_executable(mport.version_cmp ${mport.version_cmp_source_files})

target_link_libraries(mport.version_cmp libmport)