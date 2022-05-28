add_executable(model WIN32
               app.cpp
               app.h
               gen/shader_vs.h
               gen/shader_ps.h
               main.cpp)

add_custom_command(OUTPUT gen/shader_vs.h
                   COMMAND ${DXC_PATH} -T vs_6_0 ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl -E VSMain
                           -Fh shader_vs.h -Vn g_shaderVS
                   WORKING_DIRECTORY gen
                   MAIN_DEPENDENCY shader.hlsl)

add_custom_command(OUTPUT gen/shader_ps.h
                   COMMAND ${DXC_PATH} -T ps_6_0 ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl -E PSMain
                           -Fh shader_ps.h -Vn g_shaderPS
                   WORKING_DIRECTORY gen
                   MAIN_DEPENDENCY shader.hlsl)

add_custom_command(TARGET model POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/assets
                                            $<TARGET_FILE_DIR:model>/assets)

target_compile_definitions(model PRIVATE UNICODE)

target_link_libraries(model PRIVATE utils)

target_link_libraries(model PRIVATE d3d12.lib dxgi.lib)

target_include_directories(model PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(model PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(model PRIVATE WIL)
