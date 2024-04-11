cmake_minimum_required(VERSION 3.13)
project(mirisdr_source)

set(CMAKE_INSTALL_PREFIX "/usr/")
include(GNUInstallDirs)

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

if (NOT SDRPP_MODULE_CMAKE)
    set(SDRPP_MODULE_CMAKE "/usr/share/cmake/Modules/sdrpp_module.cmake")
endif ()

include(${SDRPP_MODULE_CMAKE})

target_link_libraries(mirisdr_source PRIVATE sdrpp_core mirisdr)
