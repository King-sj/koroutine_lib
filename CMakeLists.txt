cmake_minimum_required(VERSION 3.29)
project(KoroutineLibrary VERSION 0.0.1 LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# add_compile_definitions(KOROUTINE_DEBUG)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

if(NOT MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address -fno-omit-frame-pointer -Wreturn-type")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -pthread)
endif()

# Add google testing framework
include(FetchContent)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

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
add_subdirectory(demos)
add_subdirectory(cmakes)
# 安装目标
install(DIRECTORY include/ DESTINATION include)
install(TARGETS koroutinelib_static koroutinelib_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 生成聚合的 header
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/include/koroutine/koroutine.hpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/koroutine
    COMMAND ${CMAKE_COMMAND} -E env "${Python3_EXECUTABLE}" ${CMAKE_SOURCE_DIR}/scripts/generate_single_header.py ${CMAKE_SOURCE_DIR}/include/koroutine ${CMAKE_BINARY_DIR}/include/koroutine/koroutine.hpp
    DEPENDS ${CMAKE_SOURCE_DIR}/include/koroutine/*.hpp ${CMAKE_SOURCE_DIR}/scripts/generate_single_header.py
    COMMENT "Generating single header file koroutine.hpp (inlining project headers)"
)

# expose a custom target so users can run: cmake --build build --target koroutine_single_header
add_custom_target(koroutine_single_header
    DEPENDS ${CMAKE_BINARY_DIR}/include/koroutine/koroutine.hpp
    COMMENT "Target to (re)generate single-header koroutine.hpp"
)
