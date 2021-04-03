cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.info)

set(mport.info_source_files mport.info.c)
add_executable(mport.info ${mport.info_source_files})

target_link_libraries(mport.fetch libmport)