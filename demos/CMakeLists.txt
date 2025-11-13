# Demo可执行文件
# add_executable(muti_loop_demo
#     muti_loop.cpp
# )
# add_executable(channel_demo
#     channel.cpp
# )
# add_executable(add_demo
#     add.cpp
# )

# target_link_libraries(muti_loop_demo
#     PRIVATE
#     koroutinelib_static
# )
# target_link_libraries(channel_demo
#     PRIVATE
#     koroutinelib_static
# )
# target_link_libraries(add_demo
#     PRIVATE
#     koroutinelib_static
# )

# using macro to simplify demo executable creation
macro(add_koroutine_demo demo_name)
    add_executable(${demo_name}_demo
        ${demo_name}.cpp
    )
    target_link_libraries(${demo_name}_demo
        PRIVATE
        koroutinelib_static
    )
endmacro()

# find all .cpp files in the demos directory and create executables
file(GLOB DEMO_SOURCES "*.cpp")
foreach(DEMO_SOURCE ${DEMO_SOURCES})
    get_filename_component(DEMO_NAME ${DEMO_SOURCE} NAME_WE)
    add_koroutine_demo(${DEMO_NAME})
endforeach()