cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.update)

set(mport.update_source_files mport.update.c)
add_executable(mport.update ${mport.update_source_files})

target_link_libraries(mport.update libmport)