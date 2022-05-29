add_executable(raytracing WIN32
               app.cpp
               app.h
               gen/shader_src.h
               shader.h
               main.cpp)

compile_shader(OUTPUT gen/shader_src.h
               PROFILE lib_6_3
               SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/shader.hlsl
               VAR_NAME g_shaderSrc
               EXTRA_ARGS -DHLSL
               DEPENDS shader.h)

link_assets_dir(TARGET raytracing)

target_link_libraries(raytracing PRIVATE WIL)
target_link_libraries(raytracing PRIVATE DirectXMath)

target_include_directories(raytracing PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(raytracing PRIVATE d3d12.lib dxgi.lib)

target_link_libraries(raytracing PRIVATE utils)

target_include_directories(raytracing PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

get_target_property(TEST_VAR raytracing INCLUDE_DIRECTORIES)
message("${TEST_VAR}")
