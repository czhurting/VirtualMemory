cmake_minimum_required(VERSION 3.10)
project(VirtualMemory)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 按平台选源文件
if (WIN32)
    set(PLATFORM_SRCS src/UniqueMemoryForWindows.cpp)
elseif (UNIX)
    set(PLATFORM_SRCS src/UniqueMemoryForLinux.cpp)
endif()

add_library(${PROJECT_NAME} SHARED
    ${PLATFORM_SRCS}
)

add_executable(test_execute test/size.cpp )

target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
target_include_directories(test_execute
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# vm 命名空间单元测试：验证 UniqueVirtual 的 commit/decommit free-list 合并逻辑
add_executable(test_vm test/vm_test.cpp)
target_include_directories(test_vm PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_compile_definitions(test_vm PRIVATE VM_TEST)
if (MSVC)
    target_compile_options(test_vm PRIVATE /utf-8)
endif()

enable_testing()
add_test(NAME vm_test COMMAND test_vm)