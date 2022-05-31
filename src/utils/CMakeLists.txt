add_library(utils STATIC
            camera.cpp
            gltf_loader.cpp
            memory.cpp
            window.cpp
            inc/utils/camera.h
            inc/utils/gltf_loader.h
            inc/utils/memory.h
            inc/utils/window.h)

target_link_libraries(utils PRIVATE DirectXMath)
target_link_libraries(utils PRIVATE nlohmann_json)

target_include_directories(utils PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(utils PUBLIC d3d12.lib)

target_include_directories(utils PUBLIC inc)
