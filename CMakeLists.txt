cmake_minimum_required(VERSION 3.30)

# 项目名
set(PROJECT_NAME myproject)
set(PROJECT_TARGET_LIB ${PROJECT_NAME}_lib)
set(USE_STATIC ON)

# 是否去除终端
# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(NO_TERMINAL OFF)
# else ()
#     set(NO_TERMINAL ON)
# endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# vcpkg 默认链接
if(LINUX)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain" FORCE)
    set(DEFAULT_VCPKG_TRIPLET "x64-linux")
elseif(WIN32)
    set(CMAKE_TOOLCHAIN_FILE "E:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain" FORCE)
    if(USE_STATIC)
        set(DEFAULT_VCPKG_TRIPLET "x64-windows-static")
    else()
        set(DEFAULT_VCPKG_TRIPLET "x64-windows")
    endif()
endif()

project(${PROJECT_NAME})

set(VCPKG_TARGET_TRIPLET ${DEFAULT_VCPKG_TRIPLET} CACHE STRING "Vcpkg target triplet" FORCE)

message(STATUS "-----Using toolchain: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "-----Vcpkg triplet: ${VCPKG_TARGET_TRIPLET}")


# 输出目录
set(BASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")

file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

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

# 把 src/*.cpp 变为项目主库（供主程序 & 测试共享）
file(GLOB SRC_SOURCES "src/*.cpp")

add_library(${PROJECT_TARGET_LIB} ${SRC_SOURCES})

target_include_directories(${PROJECT_TARGET_LIB} PUBLIC
    ${PROJECT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/resource
)

# 第三方依赖
################################################################
################################################################
find_package(fmt REQUIRED)
target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC fmt::fmt)





################################################################
################################################################

# 在 no-terminal / Windows 情况下的宏与依赖全部放在库中（测试能继承）
if(NO_TERMINAL AND WIN32)
    target_link_libraries(${PROJECT_TARGET_LIB} PUBLIC user32 gdi32)
    target_compile_definitions(${PROJECT_TARGET_LIB} PUBLIC USE_SUB_WINDOW)
endif()


# 主程序独立编译 main.cpp（不包含源码）
add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_TARGET_LIB})


# gtest 支持、tests 目录自动扫描

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

# 自动扫描 tests/*.cpp
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

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

