# 启用测试
enable_testing()

# 找到 GTest（如果你在别处已经 find_package 过，可以移除这行）
# find_package(GTest REQUIRED)
include(GoogleTest)

# 搜索所有单元测试源文件（相对于当前 CMakeLists.txt 的路径）
file(GLOB_RECURSE UNIT_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# 创建一个函数：从单个源文件创建独立的测试 target
function(add_unit_test_from_file SRC)
    # 从文件名生成目标名，例如 unit/test_basic.cpp -> test_test_basic
    get_filename_component(TEST_NAME ${SRC} NAME_WE)
    set(TGT "${TEST_NAME}")

    add_executable(${TGT} ${SRC})

    # 链接库（按需调整）
    target_link_libraries(${TGT}
        PRIVATE
        koroutinelib_static
        GTest::gtest_main
    )

    # 使用 gtest 的自动发现并将子测试按可执行文件分组
    # 每个子测试会以 "${TGT}/<TestName>" 的形式注册到 CTest，避免重复的顶级 add_test
    gtest_discover_tests(${TGT} TEST_PREFIX "${TGT}/")
endfunction()

# 为每个找到的测试源文件调用上面的函数
foreach(SRC IN LISTS UNIT_TEST_SOURCES)
    add_unit_test_from_file(${SRC})
endforeach()

# 集成测试
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.cpp"
)

foreach(SRC IN LISTS INTEGRATION_TEST_SOURCES)
    get_filename_component(TEST_NAME ${SRC} NAME_WE)
    set(TGT "test_${TEST_NAME}")

    add_executable(${TGT} ${SRC})

    # 特殊处理 single_header 测试
    if(TEST_NAME STREQUAL "test_single_header")
        add_dependencies(${TGT} koroutine_single_header)
        target_include_directories(${TGT} PRIVATE ${CMAKE_BINARY_DIR}/include)
    endif()

    target_link_libraries(${TGT}
        PRIVATE
        koroutinelib_static
        GTest::gtest_main
    )

    # 使用 gtest 的自动发现并将子测试按可执行文件分组
    gtest_discover_tests(${TGT} TEST_PREFIX "${TGT}/")
endforeach()

