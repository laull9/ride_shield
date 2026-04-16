include(FetchContent)

set(RIDESHIELD_ONNXRUNTIME_VERSION "1.21.0" CACHE STRING "ONNX Runtime release version to download")

function(_rideshield_prepend_prefix_if_exists prefix_path)
    if(EXISTS "${prefix_path}")
        list(PREPEND CMAKE_PREFIX_PATH "${prefix_path}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    endif()
endfunction()

# ── 根据目标平台选择 ORT 预编译包 URL ──
function(_rideshield_onnxruntime_release_url out_url out_root_name)
    set(_ver "${RIDESHIELD_ONNXRUNTIME_VERSION}")

    if(CMAKE_SYSTEM_NAME STREQUAL "Windows" OR WIN32)
        set(_name "onnxruntime-win-x64-${_ver}")
        set(${out_url} "https://github.com/microsoft/onnxruntime/releases/download/v${_ver}/${_name}.zip" PARENT_SCOPE)
        set(${out_root_name} "${_name}" PARENT_SCOPE)
        return()
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR APPLE)
        # macOS universal binary
        set(_name "onnxruntime-osx-universal2-${_ver}")
        set(${out_url} "https://github.com/microsoft/onnxruntime/releases/download/v${_ver}/${_name}.tgz" PARENT_SCOPE)
        set(${out_root_name} "${_name}" PARENT_SCOPE)
        return()
    endif()

    # Linux
    if(RIDESHIELD_TARGET_ARCH MATCHES "^(aarch64|arm64)$"
       OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(_arch "aarch64")
    else()
        set(_arch "x64")
    endif()

    set(_name "onnxruntime-linux-${_arch}-${_ver}")
    set(${out_url} "https://github.com/microsoft/onnxruntime/releases/download/v${_ver}/${_name}.tgz" PARENT_SCOPE)
    set(${out_root_name} "${_name}" PARENT_SCOPE)
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

    # ── ONNX Runtime：下载 Microsoft 官方预编译包（ONNX 静态链接） ──
    _rideshield_onnxruntime_release_url(_ort_url _ort_root_name)

    FetchContent_Declare(
        onnxruntime_prebuilt
        URL "${_ort_url}"
        DOWNLOAD_EXTRACT_TIMESTAMP ON
    )
    FetchContent_MakeAvailable(onnxruntime_prebuilt)

    set(_ort_root "${onnxruntime_prebuilt_SOURCE_DIR}")

    # 查找 include 和 lib 目录
    if(EXISTS "${_ort_root}/include/onnxruntime_cxx_api.h")
        set(_ort_include "${_ort_root}/include")
    elseif(EXISTS "${_ort_root}/include/onnxruntime/onnxruntime_cxx_api.h")
        set(_ort_include "${_ort_root}/include/onnxruntime")
    else()
        set(_ort_include "")
    endif()

    if(EXISTS "${_ort_root}/lib")
        set(_ort_libdir "${_ort_root}/lib")
    else()
        set(_ort_libdir "")
    endif()

    if(_ort_include AND _ort_libdir AND NOT TARGET RideShield_onnxruntime)
        find_library(_ort_lib
            NAMES onnxruntime libonnxruntime
            PATHS "${_ort_libdir}"
            NO_DEFAULT_PATH
        )

        if(NOT _ort_lib)
            # glob fallback for versioned symlinks
            file(GLOB _ort_lib_candidates "${_ort_libdir}/libonnxruntime.so" "${_ort_libdir}/libonnxruntime.so.*"
                                          "${_ort_libdir}/libonnxruntime.dylib" "${_ort_libdir}/onnxruntime.lib")
            if(_ort_lib_candidates)
                list(SORT _ort_lib_candidates COMPARE NATURAL ORDER DESCENDING)
                list(GET _ort_lib_candidates 0 _ort_lib)
            endif()
        endif()

        if(_ort_lib)
            if(WIN32)
                find_file(_ort_dll NAMES onnxruntime.dll PATHS "${_ort_root}/lib" "${_ort_root}/bin" NO_DEFAULT_PATH)
                if(_ort_dll)
                    add_library(RideShield_onnxruntime SHARED IMPORTED)
                    set_target_properties(RideShield_onnxruntime PROPERTIES
                        IMPORTED_IMPLIB "${_ort_lib}"
                        IMPORTED_LOCATION "${_ort_dll}"
                        INTERFACE_INCLUDE_DIRECTORIES "${_ort_include}"
                    )
                endif()
            else()
                add_library(RideShield_onnxruntime UNKNOWN IMPORTED)
                set_target_properties(RideShield_onnxruntime PROPERTIES
                    IMPORTED_LOCATION "${_ort_lib}"
                    INTERFACE_INCLUDE_DIRECTORIES "${_ort_include}"
                )
            endif()
            add_library(RideShield::onnxruntime ALIAS RideShield_onnxruntime)

            # 收集运行时路径信息
            set(_ort_runtime_dirs "${_ort_libdir}")
            set(_ort_runtime_files "")

            file(GLOB _ort_runtime_candidates
                "${_ort_libdir}/libonnxruntime.so*"
                "${_ort_libdir}/libonnxruntime.dylib"
                "${_ort_libdir}/libonnxruntime_providers_shared.so*"
                "${_ort_libdir}/libonnxruntime_providers_shared.dylib"
                "${_ort_root}/bin/onnxruntime.dll"
                "${_ort_root}/bin/onnxruntime_providers_shared.dll")
            list(APPEND _ort_runtime_files ${_ort_runtime_candidates})
            list(REMOVE_DUPLICATES _ort_runtime_files)

            set(RIDESHIELD_ONNXRUNTIME_RUNTIME_DIRS "${_ort_runtime_dirs}" PARENT_SCOPE)
            set(RIDESHIELD_ONNXRUNTIME_RUNTIME_FILES "${_ort_runtime_files}" PARENT_SCOPE)

            message(STATUS "[RideShield] ONNX Runtime ${RIDESHIELD_ONNXRUNTIME_VERSION} (prebuilt): ${_ort_lib}")
        else()
            message(WARNING "[RideShield] Downloaded ORT but could not locate library in ${_ort_libdir}")
        endif()
    elseif(NOT TARGET RideShield::onnxruntime)
        message(WARNING "[RideShield] Downloaded ORT but could not locate include/lib at ${_ort_root}")
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

    if(NOT TARGET RideShield::onnxruntime)
        message(STATUS "[RideShield] ONNX Runtime not available")
    endif()

    set(_missing_deps "")
    if(NOT OpenCV_FOUND)
        list(APPEND _missing_deps "OpenCV")
    endif()
    if(_missing_deps)
        list(JOIN _missing_deps ", " _missing_deps_str)
        if(TARGET opencv_ep)
            message(STATUS "[RideShield] ${_missing_deps_str} 尚未构建。首次构建后请重新 configure。")
        else()
            message(STATUS "[RideShield] ${_missing_deps_str} 未在系统中找到。")
            message(STATUS "  可以安装系统包，或使用 -DRIDESHIELD_ENABLE_SOURCE_DEPENDENCY_TARGETS=ON 从源码构建。")
        endif()
    endif()
endfunction()