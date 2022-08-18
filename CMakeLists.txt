cmake_minimum_required(VERSION 3.13)
project(mirisdr_source)

set(CMAKE_INSTALL_PREFIX "/usr/")
include(GNUInstallDirs)

if (MSVC)
    set(CMAKE_CXX_FLAGS "-O2 /std:c++17 /EHsc")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17 -Wno-unused-command-line-argument -undefined dynamic_lookup")
else ()
    set(CMAKE_CXX_FLAGS "-O3 -std=c++17")
endif ()

file(GLOB_RECURSE SRC "src/*.cpp" "src/*.c")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")

add_library(mirisdr_source SHARED ${SRC})
target_link_libraries(mirisdr_source PRIVATE sdrpp_core mirisdr)
target_include_directories(mirisdr_source PRIVATE "src/" "/usr/include/sdrpp_core/" "/usr/include/sdrpp_core/imgui")
set_target_properties(mirisdr_source PROPERTIES PREFIX "")

# Install directives
install(TARGETS mirisdr_source DESTINATION ${CMAKE_INSTALL_LIBDIR}/sdrpp/plugins)
