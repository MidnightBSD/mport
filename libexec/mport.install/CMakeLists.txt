cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.install)

set(mport.install_source_files mport.install.c)
add_executable(mport.install ${mport.install_source_files})

target_link_libraries(mport.install libmport)