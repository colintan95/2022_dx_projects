cmake_minimum_required(VERSION 3.20)

project(dx_projects)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(/W4 /WX)
add_compile_definitions(UNICODE)

set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/wil)

add_subdirectory(external/DirectXMath)
add_subdirectory(external/json)

set(DXC_PATH ${PROJECT_SOURCE_DIR}/external/dxc/dxc.exe)

add_subdirectory(src/utils)

add_subdirectory(src/model)
add_subdirectory(src/raytracing)
