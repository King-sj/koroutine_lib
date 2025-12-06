# using macro to simplify demo executable creation
macro(add_koroutine_demo demo_name demo_source)
    add_executable(${demo_name}_demo
        ${demo_source}
    )
    target_link_libraries(${demo_name}_demo
        PRIVATE
        koroutinelib_static
    )
endmacro()

# find all .cpp files in the demos directory and create executables
file(GLOB_RECURSE DEMO_SOURCES "*.cpp")
foreach(DEMO_SOURCE ${DEMO_SOURCES})
    get_filename_component(DEMO_NAME ${DEMO_SOURCE} NAME_WE)
    add_koroutine_demo(${DEMO_NAME} ${DEMO_SOURCE})
endforeach()