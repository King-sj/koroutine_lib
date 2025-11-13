# find all .cpp files in the src directory
file(GLOB SOURCE_FILES "*.cpp")

# 生成静态库
add_library(koroutinelib_static STATIC
    ${SOURCE_FILES}
)

# 生成动态库
add_library(koroutinelib_shared SHARED
    ${SOURCE_FILES}
)

set_target_properties(koroutinelib_static PROPERTIES
    OUTPUT_NAME "koroutinelib"
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

set_target_properties(koroutinelib_shared PROPERTIES
    OUTPUT_NAME "koroutinelib"
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

target_include_directories(koroutinelib_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
