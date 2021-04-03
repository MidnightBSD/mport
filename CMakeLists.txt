cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(mport_pkg)

list (APPEND CMAKE_MODULE_PATH "${mport_pkg_SOURCE_DIR}/cmake")

include (BuildUtil)
include (CheckCCompilerFlag)
include (CheckCSourceCompiles)
include (CMakePushCheckState)

if (NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Release)
endif ()

find_package(SQLite3 REQUIRED)
if(NOT SQLITE3_FOUND)
    message(SEND_ERROR "sqlite3 dependency not found!")
endif()

find_package(BZip2 REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)

include(CheckIncludeFiles)

include(CheckCSourceCompiles)
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_C_FLAGS} /WX /W4")
else()
    set(CMAKE_REQUIRED_LIBRARIES m)
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_C_FLAGS} -Werror -Wall")
endif()

if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_REQUIRED_LIBRARIES
            "${CMAKE_REQUIRED_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}")
    check_c_source_compiles("
#include <pthread.h>
int main(int argc, char* argv[]) {
  (void)PTHREAD_MUTEX_RECURSIVE;
  (void)argv;
  return argc;
}
  " HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
    if(HAVE_PTHREAD_MUTEX_RECURSIVE_DEFN)
        add_definitions(-DHAVE_PTHREAD_MUTEX_RECURSIVE=1)
    endif()
endif()

find_package(kqueue REQUIRED)
find_package(pthread_workqueue)
find_package(BlocksRuntime)
find_package(dispatch PATHS /usr/lib)

#set (HAVE_PTHREAD_WORKQUEUES "${PTHREAD_WORKQUEUE_FOUND}")
#dispatch_search_libs(pthread_workqueue_setdispatch_np LIBRARIES
#        "${PTHREAD_WORKQUEUE_LIBRARIES}")

check_c_compiler_flag(-fblocks COMPILER_HAS_BLOCKS_SUPPORT)
check_c_compiler_flag(-momit-leaf-frame-pointer SUPPORTS_MOMIT_LEAF_FRAME_POINTER)

if (NOT COMPILER_HAS_BLOCKS_SUPPORT)
    message(WARNING "Blocks compiler support was not found; dispatch i/o "
            "will not be built.")
endif ()

dispatch_check_headers(sys/cdefs.h unistd.h)

if (HAVE_UNISTD_H AND COMPILER_HAS_BLOCKS_SUPPORT)
    cmake_push_check_state()
    set(CMAKE_REQUIRED_DEFINITIONS -D_GNU_SOURCE)
    set(CMAKE_REQUIRED_FLAGS -fblocks)
    check_c_source_compiles("#include <unistd.h>\nint main(){}"
            HAS_WORKING_UNISTD_H)
    cmake_pop_check_state()
    if (NOT HAS_WORKING_UNISTD_H)
        set (HAS_PROBLEMATIC_UNISTD_H 1)
    endif ()
endif ()

add_compile_options(
        -Wall
        -Wextra
        -Wno-unused-parameter

        -fvisibility=hidden
)

if (COMPILER_HAS_BLOCKS_SUPPORT)
    add_compile_options(-fblocks)
endif ()

if (SUPPORTS_MOMIT_LEAF_FRAME_POINTER)
    add_compile_options(-momit-leaf-frame-pointer)
endif ()

if (CMAKE_C_COMPILER_ID MATCHES "GNU")
    add_compile_options(-Wno-unknown-pragmas)
endif ()

add_subdirectory(libmport)
add_subdirectory(libexec)
add_subdirectory(mport)