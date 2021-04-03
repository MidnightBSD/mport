cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.fetch)

set(mport.fetch_source_files mport.fetch.c)
add_executable(mport.fetch ${mport.fetch_source_files})

target_link_libraries(mport.fetch libmport)