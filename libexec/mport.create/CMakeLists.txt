cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.create)

set(mport.create_source_files mport.create.c)
add_executable(mport.create ${mport.create_source_files})

target_link_libraries(mport.create mport)