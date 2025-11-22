# 启用测试
enable_testing()

# 单元测试可执行文件
add_executable(test_unit
    unit/test_basic.cpp
    unit/test_executor.cpp
    unit/test_channel.cpp
    unit/test_sync.cpp
    unit/test_async_io.cpp
    unit/test_new_features.cpp
)

# 链接库和gtest
target_link_libraries(test_unit
    PRIVATE
    koroutinelib_static
    GTest::gtest_main
)

# 添加测试用例
include(GoogleTest)
gtest_discover_tests(test_unit)
