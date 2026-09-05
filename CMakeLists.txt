cmake_minimum_required(VERSION 3.10)
project(VirtualMemory)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 按平台选源文件
if (WIN32)
    set(PLATFORM_SRCS src/MemoryForWindows.cpp)
elseif (UNIX)
    set(PLATFORM_SRCS src/MemoryForLinux.cpp)
endif()

add_library(${PROJECT_NAME} SHARED
    ${PLATFORM_SRCS}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)