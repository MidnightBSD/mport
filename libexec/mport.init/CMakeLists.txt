cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.init)

set(mport.init_source_files mport.init.c)
add_executable(mport.init ${mport.init_source_files})

target_link_libraries(mport.init libmport)