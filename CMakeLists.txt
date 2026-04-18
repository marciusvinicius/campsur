cmake_minimum_required(VERSION 3.20)

# JetBrains-friendly wrapper that delegates builds to the existing make/premake flow.
project(campsur_jetbrains NONE)

find_program(CAMP_SUR_MAKE_EXECUTABLE make REQUIRED)

function(add_make_target target_name)
  add_custom_target(
    ${target_name}
    COMMAND ${CAMP_SUR_MAKE_EXECUTABLE} config=debug_arm64 ${target_name}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
  )
endfunction()

add_custom_target(
  campsur_generate_gmake2
  COMMAND premake5 gmake2
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  USES_TERMINAL
  VERBATIM
)

add_make_target(campsur_editor)
add_make_target(campsur_game)
add_make_target(campsur_platformer)
add_make_target(campsur_tiled_walk)
add_make_target(campsursnakegame)
add_make_target(campsur_fp_plane)
add_make_target(campsur_box_fps)

add_custom_target(
  run_campsur_box_fps
  COMMAND ${CMAKE_SOURCE_DIR}/bin/Debug/campsur_box_fps
  DEPENDS campsur_box_fps
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  USES_TERMINAL
  VERBATIM
)
