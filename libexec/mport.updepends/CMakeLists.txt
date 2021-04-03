cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.updepends)

set(mport.updepends_source_files mport.updepends.c)
add_executable(mport.updepends ${mport.updepends_source_files})

target_link_libraries(mport.updepends libmport)