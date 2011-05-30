project(splib)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../../../applications/buildtools)
include(Macros)

set(SPLIB_DIR ${PROJECT_SOURCE_DIR})
include(splib.cmake)
include_directories(../../../released/interface)

# Include the generic library module 
include(Library)
