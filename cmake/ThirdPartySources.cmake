include(ExternalProject)

set(RIDESHIELD_THIRD_PARTY_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/src" CACHE PATH "Source cache for third-party dependencies")
set(RIDESHIELD_THIRD_PARTY_BUILD_DIR "${CMAKE_BINARY_DIR}/third_party" CACHE PATH "Build directory for third-party dependencies")
set(RIDESHIELD_THIRD_PARTY_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/install" CACHE PATH "Install directory for third-party dependencies")
set(RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE "" CACHE FILEPATH "Toolchain file forwarded to dependency builds")
set(RIDESHIELD_ONNXRUNTIME_C_COMPILER "" CACHE FILEPATH "Override C compiler for the onnxruntime external build")
set(RIDESHIELD_ONNXRUNTIME_CXX_COMPILER "" CACHE FILEPATH "Override C++ compiler for the onnxruntime external build")
set(RIDESHIELD_ONNXRUNTIME_ASM_COMPILER "" CACHE FILEPATH "Override ASM compiler for the onnxruntime external build")
set(RIDESHIELD_ONNXRUNTIME_ISOLATE_VCPKG ON CACHE BOOL "Unset vcpkg-related environment variables for the onnxruntime external build")
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

    set(_onnxruntime_c_compiler "${RIDESHIELD_ONNXRUNTIME_C_COMPILER}")
    set(_onnxruntime_cxx_compiler "${RIDESHIELD_ONNXRUNTIME_CXX_COMPILER}")
    set(_onnxruntime_asm_compiler "${RIDESHIELD_ONNXRUNTIME_ASM_COMPILER}")

    if(NOT _onnxruntime_c_compiler)
        set(_onnxruntime_c_compiler "${CMAKE_C_COMPILER}")
    endif()
    if(NOT _onnxruntime_cxx_compiler)
        set(_onnxruntime_cxx_compiler "${CMAKE_CXX_COMPILER}")
    endif()
    if(NOT _onnxruntime_asm_compiler AND NOT MSVC)
        set(_onnxruntime_asm_compiler "${_onnxruntime_c_compiler}")
    endif()

    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux"
       AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
       AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15
       AND NOT RIDESHIELD_ONNXRUNTIME_C_COMPILER
       AND NOT RIDESHIELD_ONNXRUNTIME_CXX_COMPILER)
        find_program(_rideshield_gcc14 gcc-14)
        find_program(_rideshield_gxx14 g++-14)
        find_program(_rideshield_clang clang)
        find_program(_rideshield_clangxx clang++)

        if(_rideshield_gcc14 AND _rideshield_gxx14)
            set(_onnxruntime_c_compiler "${_rideshield_gcc14}")
            set(_onnxruntime_cxx_compiler "${_rideshield_gxx14}")
            if(NOT RIDESHIELD_ONNXRUNTIME_ASM_COMPILER)
                set(_onnxruntime_asm_compiler "${_rideshield_gcc14}")
            endif()
            message(STATUS "[RideShield] GCC ${CMAKE_CXX_COMPILER_VERSION} triggers protobuf ICE in onnxruntime; using gcc-14/g++-14 for onnxruntime_ep.")
        elseif(_rideshield_clang AND _rideshield_clangxx)
            set(_onnxruntime_c_compiler "${_rideshield_clang}")
            set(_onnxruntime_cxx_compiler "${_rideshield_clangxx}")
            if(NOT RIDESHIELD_ONNXRUNTIME_ASM_COMPILER)
                set(_onnxruntime_asm_compiler "${_rideshield_clang}")
            endif()
            message(STATUS "[RideShield] GCC ${CMAKE_CXX_COMPILER_VERSION} triggers protobuf ICE in onnxruntime; using clang/clang++ for onnxruntime_ep.")
        endif()
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

    set(_onnxruntime_build_dir "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/onnxruntime-out")
    set(_onnxruntime_build_args
        --config ${_rideshield_external_config}
        --parallel
        --skip_tests
        --compile_no_warning_as_error
        --build_shared_lib
        --cmake_generator "${CMAKE_GENERATOR}"
        --build_dir "${_onnxruntime_build_dir}"
        --cmake_extra_defines
            CMAKE_C_COMPILER=${_onnxruntime_c_compiler}
            CMAKE_CXX_COMPILER=${_onnxruntime_cxx_compiler}
            CMAKE_POSITION_INDEPENDENT_CODE=ON
            CMAKE_DISABLE_FIND_PACKAGE_Eigen3=TRUE
            FETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER
            CMAKE_INSTALL_PREFIX=${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/onnxruntime
    )

    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND _onnxruntime_build_args --cmake_generator_platform "${CMAKE_GENERATOR_PLATFORM}")
    endif()

    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND _onnxruntime_build_args --cmake_extra_defines CMAKE_GENERATOR_TOOLSET=${CMAKE_GENERATOR_TOOLSET})
    endif()

    if(MSVC AND CMAKE_MSVC_RUNTIME_LIBRARY)
        list(APPEND _onnxruntime_build_args --cmake_extra_defines CMAKE_MSVC_RUNTIME_LIBRARY=${CMAKE_MSVC_RUNTIME_LIBRARY})
    endif()

    if(_onnxruntime_asm_compiler)
        list(APPEND _onnxruntime_build_args --cmake_extra_defines CMAKE_ASM_COMPILER=${_onnxruntime_asm_compiler})
    endif()

    if(CMAKE_MAKE_PROGRAM)
        list(APPEND _onnxruntime_build_args --cmake_extra_defines CMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM})
    endif()

    if(RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE)
        list(APPEND _onnxruntime_build_args --cmake_extra_defines CMAKE_TOOLCHAIN_FILE=${RIDESHIELD_DEPENDENCY_TOOLCHAIN_FILE})
    endif()

    if(RIDESHIELD_TARGET_ARCH MATCHES "^(aarch64|arm64)$")
        list(APPEND _onnxruntime_build_args --arm64)
    endif()

    set(_onnxruntime_build_command ${CMAKE_COMMAND} -E env)
    if(RIDESHIELD_ONNXRUNTIME_ISOLATE_VCPKG)
        list(APPEND _onnxruntime_build_command
            --unset=CMAKE_PROJECT_TOP_LEVEL_INCLUDES
            --unset=CMAKE_TOOLCHAIN_FILE
            --unset=VCPKG_DEFAULT_TRIPLET
            --unset=VCPKG_FEATURE_FLAGS
            --unset=VCPKG_INSTALLATION_ROOT
            --unset=VCPKG_MANIFEST_MODE
            --unset=VCPKG_OVERLAY_PORTS
            --unset=VCPKG_OVERLAY_TRIPLETS
            --unset=VCPKG_ROOT)
    endif()

    if(WIN32)
        list(APPEND _onnxruntime_build_command cmd /c build.bat)
    else()
        list(APPEND _onnxruntime_build_command ./build.sh)
    endif()
    list(APPEND _onnxruntime_build_command ${_onnxruntime_build_args})

    set(_onnxruntime_clean_command
        ${CMAKE_COMMAND} -E rm -rf
        "${_onnxruntime_build_dir}"
        "${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/onnxruntime")

    set(_onnxruntime_stage_command
        ${CMAKE_COMMAND}
        -DSOURCE_DIR=${RIDESHIELD_THIRD_PARTY_SOURCE_DIR}/onnxruntime
        -DBUILD_DIR=${_onnxruntime_build_dir}
        -DINSTALL_DIR=${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/onnxruntime
        -P "${_rideshield_third_party_module_dir}/scripts/stage_onnxruntime.cmake")

    ExternalProject_Add(onnxruntime_ep
        PREFIX "${RIDESHIELD_THIRD_PARTY_BUILD_DIR}/onnxruntime-prefix"
        GIT_REPOSITORY https://github.com/microsoft/onnxruntime.git
        GIT_TAG v1.20.1
        GIT_SHALLOW ON
        UPDATE_DISCONNECTED ON
        SOURCE_DIR "${RIDESHIELD_THIRD_PARTY_SOURCE_DIR}/onnxruntime"
        CONFIGURE_COMMAND ""
        BUILD_IN_SOURCE TRUE
        BUILD_COMMAND ${_onnxruntime_clean_command} COMMAND ${_onnxruntime_build_command}
        INSTALL_COMMAND ${_onnxruntime_stage_command}
    )
    ExternalProject_Add_StepTargets(onnxruntime_ep download)

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
            onnxruntime_ep-download
            ros2_bootstrap_ep-download
    )

    add_custom_target(deps-build
        DEPENDS
            opencv_ep
            onnxruntime_ep
            ros2_bootstrap_ep
    )
endfunction()