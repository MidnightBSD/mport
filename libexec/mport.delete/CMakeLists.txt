cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.delete)

set(mport.delete_source_files mport.delete.c)
add_executable(mport.delete ${mport.delete_source_files})

target_link_libraries(mport.delete libmport)