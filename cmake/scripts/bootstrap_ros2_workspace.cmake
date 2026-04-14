if(NOT DEFINED ROS2_SOURCE_DIR)
    message(FATAL_ERROR "ROS2_SOURCE_DIR is required")
endif()

if(NOT DEFINED ROS2_WORKSPACE_DIR)
    message(FATAL_ERROR "ROS2_WORKSPACE_DIR is required")
endif()

set(_ros2_src_dir "${ROS2_WORKSPACE_DIR}/src")
set(_ros2_overlay_dir "${_ros2_src_dir}/ros2")

file(MAKE_DIRECTORY "${_ros2_src_dir}")
execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ROS2_SOURCE_DIR}" "${_ros2_overlay_dir}"
    COMMAND_ERROR_IS_FATAL ANY
)

set(_bootstrap_script "${ROS2_WORKSPACE_DIR}/bootstrap_ros2_sources.sh")
file(WRITE "${_bootstrap_script}" "#!/usr/bin/env bash\nset -euo pipefail\ncd \"$(dirname \"$0\")\"\nif ! command -v vcs >/dev/null 2>&1; then\n  echo \"vcs not found. Install python3-vcstool first.\" >&2\n  exit 1\nfi\nvcs import src < src/ros2/ros2.repos\n")

execute_process(COMMAND chmod +x "${_bootstrap_script}")

find_program(VCS_EXECUTABLE vcs)
if(VCS_EXECUTABLE)
    execute_process(
        COMMAND "${VCS_EXECUTABLE}" import "${_ros2_src_dir}"
        INPUT_FILE "${_ros2_overlay_dir}/ros2.repos"
        WORKING_DIRECTORY "${ROS2_WORKSPACE_DIR}"
        COMMAND_ECHO STDOUT
        RESULT_VARIABLE _vcs_result
    )

    if(NOT _vcs_result EQUAL 0)
        message(WARNING "vcs import failed. Run ${_bootstrap_script} manually after installing missing host tools.")
    endif()
else()
    message(STATUS "vcs not found. Generated ${_bootstrap_script} for later ROS2 source bootstrap.")
endif()

if(DEFINED ROS2_TOOLCHAIN_FILE AND NOT ROS2_TOOLCHAIN_FILE STREQUAL "")
    file(WRITE "${ROS2_WORKSPACE_DIR}/colcon-defaults.yaml"
"build:\n  cmake-args:\n    - -DCMAKE_TOOLCHAIN_FILE=${ROS2_TOOLCHAIN_FILE}\n")
endif()