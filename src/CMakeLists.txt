# find all .cpp files in the src directory and subdirectories
file(GLOB_RECURSE SOURCE_FILES "*.cpp")

if(WIN32)
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/linux/")
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/macos/")
elseif(APPLE)
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/linux/")
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/win/")
elseif(UNIX)
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/macos/")
    list(FILTER SOURCE_FILES EXCLUDE REGEX "/win/")
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    find_path(LIBURING_INCLUDE_DIR liburing.h)
    find_library(LIBURING_LIBRARY uring)

    if(LIBURING_INCLUDE_DIR AND LIBURING_LIBRARY)
        set(LIBURING_FOUND TRUE)
        set(LIBURING_INCLUDE_DIRS ${LIBURING_INCLUDE_DIR})
        set(LIBURING_LIBRARIES ${LIBURING_LIBRARY})
        message(STATUS "Found liburing: ${LIBURING_LIBRARY}")
    else()
        message(FATAL_ERROR "liburing not found. Please install liburing-dev.")
    endif()
endif()

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

if(LIBURING_FOUND)
    target_include_directories(koroutinelib_static PRIVATE ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(koroutinelib_static PRIVATE ${LIBURING_LIBRARIES})

    target_include_directories(koroutinelib_shared PRIVATE ${LIBURING_INCLUDE_DIRS})
    target_link_libraries(koroutinelib_shared PRIVATE ${LIBURING_LIBRARIES})
endif()
