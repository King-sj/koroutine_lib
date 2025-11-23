# 启用测试
enable_testing()

# 找到 GTest（如果你在别处已经 find_package 过，可以移除这行）
find_package(GTest REQUIRED)
include(GoogleTest)

# 搜索所有单元测试源文件（相对于当前 CMakeLists.txt 的路径）
file(GLOB_RECURSE UNIT_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

# 创建一个函数：从单个源文件创建独立的测试 target
function(add_unit_test_from_file SRC)
    # 从文件名生成目标名，例如 unit/test_basic.cpp -> test_test_basic
    get_filename_component(TEST_NAME ${SRC} NAME_WE)
    set(TGT "test_${TEST_NAME}")

    add_executable(${TGT} ${SRC})

    # 链接库（按需调整）
    target_link_libraries(${TGT}
        PRIVATE
        koroutinelib_static
        GTest::gtest_main
    )

    # 将可执行文件注册为 CTest 测试，并启用 gtest 的 test discovery
    add_test(NAME ${TGT} COMMAND ${TGT})
    gtest_discover_tests(${TGT})
endfunction()

# 为每个找到的测试源文件调用上面的函数
foreach(SRC IN LISTS UNIT_TEST_SOURCES)
    add_unit_test_from_file(${SRC})
endforeach()
