cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport.check-fake)

set(mport.check-fake_source_files mport.check-fake.c)
add_executable(mport.check-fake ${mport.check-fake_source_files})

target_link_libraries(mport.check-fake mport)