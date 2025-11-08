cmake_minimum_required(VERSION 4.0)
project(KoroutineLibrary VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

if(MSVC)
    add_compile_options(/W4 /permissive- /await)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -pthread)
endif()

# Add google testing framework
include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
    GIT_TAG        v1.14.0  # 使用稳定版本
    GIT_SHALLOW    TRUE
)

# Windows特定设置
if(WIN32)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(googletest)

add_subdirectory(src)
add_subdirectory(tests)

# 安装目标
install(DIRECTORY include/ DESTINATION include)
install(TARGETS koroutinelib_static koroutinelib_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)