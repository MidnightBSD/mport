cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.check-for-older)

set(mport.check-for-older_source_files mport.check-for-older.c)
add_executable(mport.check-for-older ${mport.check-for-older_source_files})

target_link_libraries(mport.check-for-older mport)

target_include_directories(mport PUBLIC ../../libmport)