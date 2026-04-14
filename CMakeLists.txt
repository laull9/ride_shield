cmake_minimum_required(VERSION 3.30)

set(PROJECT_NAME RideShield)
set(PROJECT_TARGET_LIB ${PROJECT_NAME}_lib)
option(USE_STATIC "Link libstdc++/libgcc statically when possible" ON)
option(RIDESHIELD_ENABLE_SOURCE_DEPENDENCY_TARGETS "Create source-fetch and superbuild targets for third-party dependencies" ON)

project(${PROJECT_NAME} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ThirdPartySources)
include(ProjectDependencies)

rideshield_enable_source_dependency_targets()
rideshield_resolve_dependencies()

set(BASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")

file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

function(rideshield_configure_runtime_support target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(RIDESHIELD_ONNXRUNTIME_RUNTIME_DIRS AND NOT WIN32)
        set_property(TARGET ${target_name} APPEND PROPERTY BUILD_RPATH "${RIDESHIELD_ONNXRUNTIME_RUNTIME_DIRS}")
    endif()

    if(WIN32 AND RIDESHIELD_ONNXRUNTIME_RUNTIME_FILES)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RIDESHIELD_ONNXRUNTIME_RUNTIME_FILES} $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

if(USE_STATIC)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(STATIC_LINK_FLAGS "-static-libgcc -static-libstdc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_LINK_FLAGS}")
    endif()
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/resource
)

file(GLOB_RECURSE SRC_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/*.cpp")

if(NOT TARGET RideShield::onnxruntime)
    list(FILTER SRC_SOURCES EXCLUDE REGEX ".*/(onnx_zero_copy|model_pipeline|yolo_detector|front_perception|rear_perception)\\.cpp$")
endif()

if(NOT RIDESHIELD_HAS_ROS2)
    list(FILTER SRC_SOURCES EXCLUDE REGEX ".*/ros2/.*\\.cpp$")
endif()

add_library(${PROJECT_TARGET_LIB} ${SRC_SOURCES})

target_include_directories(${PROJECT_TARGET_LIB} PUBLIC
    ${PROJECT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/resource
)

target_compile_features(${PROJECT_TARGET_LIB} PUBLIC cxx_std_23)

target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC fmt::fmt)

if(RIDESHIELD_HAS_OPENCV)
    target_include_directories(${PROJECT_TARGET_LIB} PUBLIC ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC ${OpenCV_LIBS})
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC RIDESHIELD_HAS_OPENCV=1)
endif()

if(TARGET RideShield::onnxruntime)
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC RideShield::onnxruntime)
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC RIDESHIELD_HAS_ONNXRUNTIME=1)
endif()

if(RIDESHIELD_HAS_ROS2)
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC rclcpp::rclcpp sensor_msgs::sensor_msgs std_msgs::std_msgs)
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC RIDESHIELD_HAS_ROS2=1)
endif()

if(NO_TERMINAL AND WIN32)
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC user32 gdi32)
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC USE_SUB_WINDOW)
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_TARGET_LIB})
rideshield_configure_runtime_support(${PROJECT_NAME})

enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW ON
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

file(GLOB TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

foreach(TEST_FILE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

    add_executable(${TEST_NAME} ${TEST_FILE})

    # 测试链接项目库
    target_link_libraries(${TEST_NAME}
        PRIVATE
            ${PROJECT_TARGET_LIB}
            GTest::gtest_main
    )

    target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)
    target_compile_definitions(${TEST_NAME} PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    rideshield_configure_runtime_support(${TEST_NAME})

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

