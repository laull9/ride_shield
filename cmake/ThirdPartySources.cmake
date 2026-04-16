include(ExternalProject)

set(RIDESHIELD_THIRD_PARTY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/src" CACHE PATH "Source cache for third-party dependencies")
set(RIDESHIELD_THIRD_PARTY_BUILD_DIR "${CMAKE_BINARY_DIR}/third_party" CACHE PATH "Build directory for third-party dependencies")
set(RIDESHIELD_THIRD_PARTY_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/install" CACHE PATH "Install directory for third-party dependencies")
set(RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE "" CACHE FILEPATH "Toolchain file forwarded to dependency builds")
set(RIDESHIELD_TARGET_ARCH "${CMAKE_SYSTEM_PROCESSOR}" CACHE STRING "Target architecture forwarded to dependency builds")
set(_rideshield_third_party_module_dir "${CMAKE_CURRENT_LIST_DIR}")

function(_rideshield_select_external_config out_var)
    if(CMAKE_CONFIGURATION_TYPES)
        set(_config_candidates ${CMAKE_CONFIGURATION_TYPES})
        if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE IN_LIST _config_candidates)
            set(${out_var} "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
            return()
        endif()
        if("Debug" IN_LIST _config_candidates)
            set(${out_var} "Debug" PARENT_SCOPE)
            return()
        endif()
        list(GET _config_candidates 0 _first_config)
        set(${out_var} "${_first_config}" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_BUILD_TYPE)
        set(${out_var} "${CMAKE_BUILD_TYPE}" PARENT_SCOPE)
    else()
        set(${out_var} "Release" PARENT_SCOPE)
    endif()
endfunction()

function(rideshield_enable_source_dependency_targets)
    if(NOT RIDESHIELD_ENABLE_SOURCE_DEPENDENCY_TARGETS)
        return()
    endif()

    file(MAKE_DIRECTORY "${RIDESHIELD_THIRD_PARTY_SOURCE_DIR}")
    file(MAKE_DIRECTORY "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}")
    file(MAKE_DIRECTORY "${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}")

    _rideshield_select_external_config(_rideshield_external_config)

    set(_common_cmake_args
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON
        -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
    )

    if(CMAKE_CONFIGURATION_TYPES)
        list(APPEND _common_cmake_args -DCMAKE_CONFIGURATION_TYPES=${CMAKE_CONFIGURATION_TYPES})
    else()
        list(APPEND _common_cmake_args -DCMAKE_BUILD_TYPE=${_rideshield_external_config})
    endif()

    if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND _common_cmake_args -DCMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
    endif()

    if(RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE)
        list(APPEND _common_cmake_args -DCMAKE_TOOLCHAIN_FILE=${RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE})
    endif()

    ExternalProject_Add(opencv_ep
        PREFIX "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/opencv-prefix"
        GIT_REPOSITORY https://github.com/opencv/opencv.git
        GIT_TAG 4.10.0
        GIT_SHALLOW ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${RIDESHIELD_THIRD_PARTY_SOURCE_DIR}/opencv"
        BINARY_DIR "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/opencv-build"
        INSTALL_DIR "${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/opencv"
        CMAKE_GENERATOR "${CMAKE_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${CMAKE_GENERATOR_PLATFORM}"
        CMAKE_GENERATOR_TOOLSET "${CMAKE_GENERATOR_TOOLSET}"
        CMAKE_ARGS
            ${_common_cmake_args}
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTS=OFF
            -DBUILD_PERF_TESTS=OFF
            -DBUILD_EXAMPLES=OFF
            -DBUILD_opencv_java=OFF
            -DBUILD_opencv_python=OFF
            -DWITH_IPP=OFF
            -DWITH_TBB=ON
            -DWITH_1394=OFF
            -DWITH_FFMPEG=OFF
            -DWITH_GSTREAMER=OFF
            -DWITH_QT=OFF
            -DWITH_GTK=OFF
            -DWITH_WEBP=OFF
            -DWITH_TIFF=OFF
            -DWITH_LZMA=OFF
            -DBUILD_opencv_apps=OFF
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${_rideshield_external_config}
        INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${_rideshield_external_config} --target install
        BUILD_BYPRODUCTS
            "${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/opencv/lib"
    )
    ExternalProject_Add_StepTargets(opencv_ep download)

    ExternalProject_Add(ros2_bootstrap_ep
        PREFIX "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/ros2-prefix"
        GIT_REPOSITORY https://github.com/ros2/ros2.git
        GIT_TAG humble
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL TRUE
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${RIDESHIELD_THIRD_PARTY_SOURCE_DIR}/ros2"
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ${CMAKE_COMMAND}
            -DROS2_SOURCE_DIR=<SOURCE_DIR>
            -DROS2_WORKSPACE_DIR=${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/ros2_ws
            -DROS2_TOOLCHAIN_FILE=${RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE}
            -P "${_rideshield_third_party_module_dir}/scripts/bootstrap_ros2_workspace.cmake"
        INSTALL_COMMAND ""
    )
    ExternalProject_Add_StepTargets(ros2_bootstrap_ep download)

    add_custom_target(deps-fetch
        DEPENDS
            opencv_ep-download
            ros2_bootstrap_ep-download
    )

    add_custom_target(deps-build
        DEPENDS
            opencv_ep
            ros2_bootstrap_ep
    )
endfunction()