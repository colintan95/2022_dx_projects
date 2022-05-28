cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

project(dx_projects)

set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/wil)

set(DXC_PATH ${PROJECT_SOURCE_DIR}/external/dxc/dxc.exe)

add_subdirectory(src/triangle)
