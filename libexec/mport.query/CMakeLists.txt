cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.query)

set(mport.query_source_files mport.query.c)
add_executable(mport.query ${mport.query_source_files})

target_link_libraries(mport.query libmport)