add_executable(model WIN32
               app.cpp
               app.h
               gen/shader_vs.h
               gen/shader_ps.h
               main.cpp)

compile_shader(OUTPUT gen/shader_vs.h
               PROFILE vs_6_0
               SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl
               ENTRY_POINT VSMain
               VAR_NAME g_shaderVS)

compile_shader(OUTPUT gen/shader_ps.h
               PROFILE ps_6_0
               SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl
               ENTRY_POINT PSMain
               VAR_NAME g_shaderPS)

link_assets_dir(TARGET model)

target_link_libraries(model PRIVATE DirectXMath)
target_link_libraries(model PRIVATE WIL)

target_include_directories(model PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(model PRIVATE d3d12.lib dxgi.lib)

target_link_libraries(model PRIVATE utils)

target_include_directories(model PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
