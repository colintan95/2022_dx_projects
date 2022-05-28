add_library(utils STATIC
            gltf_loader.cpp
            memory.cpp
            inc/utils/gltf_loader.h
            inc/utils/memory.h)

target_include_directories(utils PUBLIC inc)

target_include_directories(utils PRIVATE ${PROJECT_SOURCE_DIR}/external/d3dx12)

target_link_libraries(utils PRIVATE nlohmann_json)
target_link_libraries(utils PUBLIC d3d12.lib)
