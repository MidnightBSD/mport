include (FindPackageHandleStandardArgs)
include (CheckFunctionExists)

find_package(Threads)

find_path(DISPATCH_INCLUDE_DIRS dispatch.h PATH_SUFFIXES dispatch)

find_library(_DISPATCH_LIB "dispatch")
mark_as_advanced(_DISPATCH_LIB)

set (DISPATCH_LIBRARIES "${_DISPATCH_LIB}" ${CMAKE_THREAD_LIBS_INIT}
        CACHE STRING "Libraries to link libdispatch")

find_package_handle_standard_args(dispatch DEFAULT_MSG
        DISPATCH_LIBRARIES
        DISPATCH_INCLUDE_DIRS
        THREADS_FOUND
        )