if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BUILD_DIR OR NOT DEFINED INSTALL_DIR)
    message(FATAL_ERROR "SOURCE_DIR, BUILD_DIR and INSTALL_DIR are required")
endif()

set(_candidate_dirs
    "${BUILD_DIR}"
    "${BUILD_DIR}/Debug"
    "${BUILD_DIR}/Release"
    "${BUILD_DIR}/RelWithDebInfo"
    "${BUILD_DIR}/MinSizeRel")

set(_artifact_dir "")
foreach(_candidate IN LISTS _candidate_dirs)
    file(GLOB _candidate_link_artifacts LIST_DIRECTORIES false
        "${_candidate}/onnxruntime.lib"
        "${_candidate}/libonnxruntime.so"
        "${_candidate}/libonnxruntime.so.*"
        "${_candidate}/libonnxruntime.dylib")
    if(_candidate_link_artifacts)
        set(_artifact_dir "${_candidate}")
        break()
    endif()
endforeach()

if(NOT _artifact_dir)
    message(FATAL_ERROR "Unable to locate ONNX Runtime link artifacts under ${BUILD_DIR}")
endif()

if(NOT EXISTS "${SOURCE_DIR}/include")
    message(FATAL_ERROR "Unable to locate ONNX Runtime headers under ${SOURCE_DIR}/include")
endif()

file(REMOVE_RECURSE "${INSTALL_DIR}")
file(MAKE_DIRECTORY "${INSTALL_DIR}/include")
file(MAKE_DIRECTORY "${INSTALL_DIR}/lib")
file(MAKE_DIRECTORY "${INSTALL_DIR}/bin")

file(COPY "${SOURCE_DIR}/include/" DESTINATION "${INSTALL_DIR}/include")

file(GLOB _link_artifacts LIST_DIRECTORIES false
    "${_artifact_dir}/onnxruntime.lib"
    "${_artifact_dir}/libonnxruntime.so"
    "${_artifact_dir}/libonnxruntime.so.*"
    "${_artifact_dir}/libonnxruntime.dylib")

if(NOT _link_artifacts)
    message(FATAL_ERROR "Unable to stage ONNX Runtime libraries from ${_artifact_dir}")
endif()

foreach(_artifact IN LISTS _link_artifacts)
    file(COPY "${_artifact}" DESTINATION "${INSTALL_DIR}/lib")
endforeach()

file(GLOB _runtime_artifacts LIST_DIRECTORIES false
    "${_artifact_dir}/onnxruntime.dll"
    "${_artifact_dir}/onnxruntime_providers_shared.dll"
    "${_artifact_dir}/libonnxruntime_providers_shared.so"
    "${_artifact_dir}/libonnxruntime_providers_shared.so.*"
    "${_artifact_dir}/libonnxruntime_providers_shared.dylib")

foreach(_artifact IN LISTS _runtime_artifacts)
    get_filename_component(_extension "${_artifact}" LAST_EXT)
    if(_extension STREQUAL ".dll")
        file(COPY "${_artifact}" DESTINATION "${INSTALL_DIR}/bin")
    else()
        file(COPY "${_artifact}" DESTINATION "${INSTALL_DIR}/lib")
    endif()
endforeach()