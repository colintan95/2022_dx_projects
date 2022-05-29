cmake_minimum_required(VERSION 3.20)

project(dx_projects)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(/W4 /WX)
add_compile_definitions(UNICODE)

set(WIL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/wil)

add_subdirectory(external/DirectXMath)
add_subdirectory(external/json)

function(compile_shader)
  set(one_value_args OUTPUT PROFILE SOURCE VAR_NAME ENTRY_POINT)
  cmake_parse_arguments(SHADER "" "${one_value_args}" "" ${ARGN})

  set(extra_args "")
  if(SHADER_ENTRY_POINT)
    list(APPEND extra_args -E ${SHADER_ENTRY_POINT})
  endif(SHADER_ENTRY_POINT)

  list(JOIN extra_args " " extra_args)

  set(dxc_path ${PROJECT_SOURCE_DIR}/external/dxc/dxc.exe)

  add_custom_command(OUTPUT ${SHADER_OUTPUT}
                     COMMAND ${dxc_path} -T ${SHADER_PROFILE} ${SHADER_SOURCE} -Fh ${SHADER_OUTPUT}
                             -Vn ${SHADER_VAR_NAME} ${extra_args}
                     MAIN_DEPENDENCY ${SHADER_SOURCE})
endfunction()

function(link_assets_dir)
  set(one_value_args TARGET)
  cmake_parse_arguments(ASSETS "" "${one_value_args}" "" ${ARGN})

  add_custom_command(TARGET ${ASSETS_TARGET} POST_BUILD
                     COMMAND ${CMAKE_COMMAND} -E create_symlink ${PROJECT_SOURCE_DIR}/assets
                                              $<TARGET_FILE_DIR:${ASSETS_TARGET}>/assets)
endfunction()

add_subdirectory(src/utils)

add_subdirectory(src/model)
add_subdirectory(src/raytracing)
