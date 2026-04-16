cmake_minimum_required(VERSION 3.30)

set(PROJECT_NAME RideShield)
set(PROJECT_TARGET_LIB ${PROJECT_NAME}_lib)
option(USE_STATIC "Link libstdc++/libgcc statically when possible" ON)
option(RIDESHIELD_ENABLE_SOURCE_DEPENDENCY_TARGETS "Create source-fetch and superbuild targets for third-party dependencies" OFF)
option(RIDESHIELD_ENABLE_UI "Build Qt6 Quick car HMI (requires Qt6 Quick)" ON)

project(${PROJECT_NAME} LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# UI bridge 源码不进 lib —— 它需要 Qt moc，由主可执行文件直接编译
list(FILTER SRC_SOURCES EXCLUDE REGEX ".*/ui/.*\\.cpp$")

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

# ── 嵌入 ONNX 模型到二进制 ──
if(TARGET RideShield::onnxruntime)
    include(EmbedResource)
    rideshield_embed_resource(${PROJECT_TARGET_LIB}
        rideshield_yolo_model "${CMAKE_CURRENT_SOURCE_DIR}/res/yolo26n.onnx"
    )
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC RIDESHIELD_HAS_EMBEDDED_MODEL=1)
endif()

if(NO_TERMINAL AND WIN32)
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC user32 gdi32)
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC USE_SUB_WINDOW)
endif()

# ── Qt6 Quick 车机 UI（默认启用） ──
set(RIDESHIELD_HAS_QT_UI OFF)
if(RIDESHIELD_ENABLE_UI)
    find_package(Qt6 6.5 QUIET COMPONENTS Quick QuickControls2)
    if(Qt6_FOUND)
        set(RIDESHIELD_HAS_QT_UI ON)
        qt_standard_project_setup(REQUIRES 6.5)
        message(STATUS "[RideShield] Qt6 ${Qt6_VERSION} detected — building with UI")
    else()
        message(STATUS "[RideShield] Qt6 Quick not found — building headless")
    endif()
endif()

if(RIDESHIELD_HAS_QT_UI)
    qt_add_executable(${PROJECT_NAME}
        main.cpp
        src/ui/vehicle_bridge.cpp
        inc/RideShield/ui/vehicle_bridge.h
    )

    set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

    qt_add_qml_module(${PROJECT_NAME}
        URI    RideShield.UI
        VERSION 1.0
        QML_FILES
            ui/qml/Main.qml
            ui/qml/Theme.qml
            ui/qml/StatusBar.qml
            ui/qml/DashboardPanel.qml
            ui/qml/DetectionPanel.qml
            ui/qml/RearViewPanel.qml
            ui/qml/MetricCard.qml
            ui/qml/ZoneIndicator.qml
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${PROJECT_TARGET_LIB}
        Qt6::Quick
        Qt6::QuickControls2
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE RIDESHIELD_HAS_QT_UI=1)
else()
    add_executable(${PROJECT_NAME} main.cpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_TARGET_LIB})
endif()

rideshield_configure_runtime_support(${PROJECT_NAME})

include(CTest)

if(BUILD_TESTING)
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
    set(RIDESHIELD_TEST_TARGETS)

    foreach(TEST_FILE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)

        add_executable(${TEST_NAME} ${TEST_FILE})

        target_link_libraries(${TEST_NAME}
            PRIVATE
                ${PROJECT_TARGET_LIB}
                GTest::gtest_main
        )

        target_compile_features(${TEST_NAME} PRIVATE cxx_std_23)
        target_compile_definitions(${TEST_NAME} PRIVATE PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
        rideshield_configure_runtime_support(${TEST_NAME})

        list(APPEND RIDESHIELD_TEST_TARGETS ${TEST_NAME})

        add_test(NAME ${TEST_NAME} COMMAND $<TARGET_FILE:${TEST_NAME}>)
        set_tests_properties(${TEST_NAME} PROPERTIES FIXTURES_REQUIRED rideshield_test_binaries)
    endforeach()

    if(RIDESHIELD_TEST_TARGETS)
        add_custom_target(rideshield_tests DEPENDS ${RIDESHIELD_TEST_TARGETS})

        add_test(
            NAME __rideshield_build_test_binaries
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target rideshield_tests --config $<CONFIG>
        )
        set_tests_properties(__rideshield_build_test_binaries PROPERTIES
            FIXTURES_SETUP rideshield_test_binaries
            LABELS internal
            RUN_SERIAL TRUE
        )
    endif()
endif()



