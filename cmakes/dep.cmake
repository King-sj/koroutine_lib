# Find Python3 interpreter for optional tools (codeviz)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(Python3_EXECUTABLE)
    add_custom_target(codeviz
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/codeviz-master/codeviz.py src include -r
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Run codeviz to generate source/header visualization"
        VERBATIM
    )
else()
    message(WARNING "Python3 interpreter not found; 'codeviz' target will not be available")
endif()



add_custom_target(graphviz
    COMMAND ${CMAKE_COMMAND} "--graphviz=dep.dot" .
    COMMAND dot -Tpng dep.dot -o ../output/img/dep.png
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# -- Doxygen-based documentation & graphs (include/call/class graphs)
option(BUILD_DOCS "Build Doxygen documentation (requires Doxygen and Graphviz)" ON)

find_package(Doxygen QUIET)
if(BUILD_DOCS AND DOXYGEN_FOUND)
    # Configure Doxyfile from template
    set(DOXYGEN_IN ${CMAKE_SOURCE_DIR}/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_BINARY_DIR}/Doxyfile)
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM
    )
elseif(BUILD_DOCS)
    message(STATUS "Doxygen not found - documentation target 'doc' will not be available. Install Doxygen and Graphviz to enable detailed graphs.")
endif()

