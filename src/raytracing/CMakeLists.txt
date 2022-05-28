add_executable(raytracing WIN32
               app.cpp
               app.h
               gen/shader.h
               main.cpp)

add_custom_command(OUTPUT gen/shader.h
                   COMMAND ${DXC_PATH} -T lib_6_3 ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl
                           -Fh shader.h -Vn g_shader
                   WORKING_DIRECTORY gen
                   MAIN_DEPENDENCY shader.hlsl)

target_link_libraries(raytracing PRIVATE utils)

target_link_libraries(raytracing PRIVATE d3d12.lib dxgi.lib)

target_include_directories(raytracing PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(raytracing PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(raytracing PRIVATE WIL)
