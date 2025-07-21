cmake_minimum_required(VERSION 3.30)

# 项目名
set(PROJECT_NAME myproject)
# 是否静态运行时库
set(USE_STATIC ON)
# 是否去除终端
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(NO_TERMINAL OFF)
else ()
    set(NO_TERMINAL ON)
endif ()
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


#   设置基础输出路径
set(BASE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/out")

#   配置各类型文件的输出目录
#   可执行文件
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/bin")
#   动态库文件
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")
#   静态库文件
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BASE_OUTPUT_DIR}/lib")

#   可选：确保目录存在
file(MAKE_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
file(MAKE_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")

if(USE_STATIC)
    # 设置MSVC的静态运行时库（Windows专用）
    if(MSVC OR WIN32)
        # 强制使用静态运行时库（/MT 或 /MTd）
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    # 设置GCC/Clang的静态链接标志
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(STATIC_LINK_FLAGS "-static-libgcc -static-libstdc++")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_LINK_FLAGS}")
    endif()
endif()

include_directories(
    ${PROJECT_SOURCE_DIR}/inc
    ${PROJECT_SOURCE_DIR}/resource
)

file(GLOB SRC_SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME}
    ${SRC_SOURCES}
)

#添加头文件
if(LINUX)
    include_directories("/media/laull/E/laullc++libs")
elseif(WIN32)
    include_directories("E:/laullc++libs")
endif()

# 查找并链接fmt库
find_package(fmt REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt)


if(NO_TERMINAL)
    message(STATUS "Configuring build without terminal/console")
    
    if(WIN32)
        message(STATUS "Windows: Using sub-window approach (no console)")
        target_link_libraries(${PROJECT_NAME} PRIVATE user32 gdi32)
        # 定义宏标识使用子窗口模式
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SUB_WINDOW)
        
    elseif(APPLE)
        message(STATUS "macOS: Creating application bundle (no terminal)")
        set_target_properties(${PROJECT_NAME} PROPERTIES 
            MACOSX_BUNDLE TRUE
        )

    elseif(UNIX)
        message(STATUS "Unix/Linux: Configuring for no terminal")
        if(CMAKE_CXX_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_link_options(${PROJECT_NAME} PRIVATE -Wl)
        endif()
        
    else()
        message(WARNING "No terminal configuration not supported for this platform")
    endif()
endif()