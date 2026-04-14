include(FetchContent)

function(_rideshield_prepend_prefix_if_exists prefix_path)
    if(EXISTS "${prefix_path}")
        list(PREPEND CMAKE_PREFIX_PATH "${prefix_path}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    endif()
endfunction()

function(_rideshield_collect_onnxruntime_search_paths out_var root_path)
    set(_paths
        "${root_path}"
        "${root_path}/bin"
        "${root_path}/lib"
        "${root_path}/lib64"
        "${root_path}/Debug"
        "${root_path}/Release"
        "${root_path}/RelWithDebInfo"
        "${root_path}/MinSizeRel"
        "${root_path}/bin/Debug"
        "${root_path}/bin/Release"
        "${root_path}/bin/RelWithDebInfo"
        "${root_path}/bin/MinSizeRel"
        "${root_path}/lib/Debug"
        "${root_path}/lib/Release"
        "${root_path}/lib/RelWithDebInfo"
        "${root_path}/lib/MinSizeRel")
    list(REMOVE_DUPLICATES _paths)
    set(${out_var} "${_paths}" PARENT_SCOPE)
endfunction()

function(rideshield_resolve_dependencies)
    set(RIDESHIELD_HAS_OPENCV OFF PARENT_SCOPE)
    set(RIDESHIELD_HAS_ROS2 OFF PARENT_SCOPE)
    set(RIDESHIELD_ONNXRUNTIME_RUNTIME_DIRS "" PARENT_SCOPE)
    set(RIDESHIELD_ONNXRUNTIME_RUNTIME_FILES "" PARENT_SCOPE)

    _rideshield_prepend_prefix_if_exists("${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/opencv")

    find_package(fmt QUIET)
    if(NOT fmt_FOUND)
        FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 11.0.2
            GIT_SHALLOW ON
        )
        FetchContent_MakeAvailable(fmt)
    endif()

    find_package(OpenCV QUIET COMPONENTS core imgproc imgcodecs videoio highgui)
    if(OpenCV_FOUND)
        set(RIDESHIELD_HAS_OPENCV ON PARENT_SCOPE)
        set(OpenCV_INCLUDE_DIRS "${OpenCV_INCLUDE_DIRS}" PARENT_SCOPE)
        set(OpenCV_LIBS "${OpenCV_LIBS}" PARENT_SCOPE)
    endif()

    set(_onnxruntime_root "${RIDESHIELD_THIRD_PARTY_INSTALL_DIR}/onnxruntime")
    _rideshield_collect_onnxruntime_search_paths(_onnxruntime_search_paths "${_onnxruntime_root}")

    find_path(ONNXRUNTIME_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            "${_onnxruntime_root}/include"
            "${_onnxruntime_root}/include/onnxruntime/core/session"
        PATH_SUFFIXES onnxruntime/core/session
        NO_DEFAULT_PATH
    )
    find_library(ONNXRUNTIME_LIBRARY
        NAMES onnxruntime libonnxruntime
        PATHS ${_onnxruntime_search_paths}
        NO_DEFAULT_PATH
    )

    find_file(ONNXRUNTIME_RUNTIME_LIBRARY
        NAMES onnxruntime.dll libonnxruntime.so libonnxruntime.so.1 libonnxruntime.dylib
        PATHS ${_onnxruntime_search_paths}
        NO_DEFAULT_PATH
    )
    find_file(ONNXRUNTIME_PROVIDERS_SHARED
        NAMES onnxruntime_providers_shared.dll libonnxruntime_providers_shared.so libonnxruntime_providers_shared.so.1 libonnxruntime_providers_shared.dylib
        PATHS ${_onnxruntime_search_paths}
        NO_DEFAULT_PATH
    )

    if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIBRARY AND NOT TARGET RideShield_onnxruntime)
        get_filename_component(_onnxruntime_library_ext "${ONNXRUNTIME_LIBRARY}" EXT)
        if(WIN32 AND ONNXRUNTIME_RUNTIME_LIBRARY AND _onnxruntime_library_ext STREQUAL ".lib")
            add_library(RideShield_onnxruntime SHARED IMPORTED)
            set_target_properties(RideShield_onnxruntime PROPERTIES
                IMPORTED_IMPLIB "${ONNXRUNTIME_LIBRARY}"
                IMPORTED_LOCATION "${ONNXRUNTIME_RUNTIME_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
            )
        else()
            add_library(RideShield_onnxruntime UNKNOWN IMPORTED)
            set_target_properties(RideShield_onnxruntime PROPERTIES
                IMPORTED_LOCATION "${ONNXRUNTIME_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_INCLUDE_DIR}"
            )
        endif()
        add_library(RideShield::onnxruntime ALIAS RideShield_onnxruntime)

        set(_onnxruntime_runtime_dirs "")
        set(_onnxruntime_runtime_files "")

        if(ONNXRUNTIME_RUNTIME_LIBRARY)
            get_filename_component(_onnxruntime_runtime_dir "${ONNXRUNTIME_RUNTIME_LIBRARY}" DIRECTORY)
            list(APPEND _onnxruntime_runtime_dirs "${_onnxruntime_runtime_dir}")
            list(APPEND _onnxruntime_runtime_files "${ONNXRUNTIME_RUNTIME_LIBRARY}")
        elseif(ONNXRUNTIME_LIBRARY)
            get_filename_component(_onnxruntime_runtime_dir "${ONNXRUNTIME_LIBRARY}" DIRECTORY)
            list(APPEND _onnxruntime_runtime_dirs "${_onnxruntime_runtime_dir}")
        endif()

        if(ONNXRUNTIME_PROVIDERS_SHARED)
            get_filename_component(_onnxruntime_providers_dir "${ONNXRUNTIME_PROVIDERS_SHARED}" DIRECTORY)
            list(APPEND _onnxruntime_runtime_dirs "${_onnxruntime_providers_dir}")
            list(APPEND _onnxruntime_runtime_files "${ONNXRUNTIME_PROVIDERS_SHARED}")
        endif()

        list(REMOVE_DUPLICATES _onnxruntime_runtime_dirs)
        list(REMOVE_DUPLICATES _onnxruntime_runtime_files)
        set(RIDESHIELD_ONNXRUNTIME_RUNTIME_DIRS "${_onnxruntime_runtime_dirs}" PARENT_SCOPE)
        set(RIDESHIELD_ONNXRUNTIME_RUNTIME_FILES "${_onnxruntime_runtime_files}" PARENT_SCOPE)
    endif()

    find_package(rclcpp QUIET)
    find_package(sensor_msgs QUIET)
    find_package(std_msgs QUIET)
    if(rclcpp_FOUND AND sensor_msgs_FOUND AND std_msgs_FOUND)
        set(RIDESHIELD_HAS_ROS2 ON PARENT_SCOPE)
    endif()

    if(OpenCV_FOUND)
        message(STATUS "[RideShield] OpenCV detected: ${OpenCV_VERSION}")
    else()
        message(STATUS "[RideShield] OpenCV not detected during configure")
    endif()

    if(TARGET RideShield::onnxruntime)
        message(STATUS "[RideShield] ONNX Runtime detected: ${ONNXRUNTIME_LIBRARY}")
    else()
        message(STATUS "[RideShield] ONNX Runtime not detected during configure")
    endif()

    # 提示：如果 EP target 存在但依赖未找到，说明需要先构建再重新 configure
    set(_missing_deps "")
    if(NOT OpenCV_FOUND AND TARGET opencv_ep)
        list(APPEND _missing_deps "OpenCV")
    endif()
    if(NOT TARGET RideShield::onnxruntime AND TARGET onnxruntime_ep)
        list(APPEND _missing_deps "ONNXRuntime")
    endif()
    if(_missing_deps)
        list(JOIN _missing_deps ", " _missing_deps_str)
        message(STATUS "[RideShield] ${_missing_deps_str} 尚未构建。首次构建后请重新 configure：")
        message(STATUS "  cmake --build <build_dir>        # 构建 third-party 依赖")
        message(STATUS "  cmake --preset host-debug        # 重新 configure 以发现已安装的依赖")
        message(STATUS "  cmake --build <build_dir>        # 重新构建主项目")
    endif()
endfunction()