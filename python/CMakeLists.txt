include(GNUInstallDirs)

find_package(Python 3.6 REQUIRED
    COMPONENTS Interpreter Development.Module
)

add_library(
    ${PROJECT_NAME}-python-wrapper SHARED
        python_wrapper.c
        ${CMAKE_SOURCE_DIR}/src/crc.c
        ${CMAKE_SOURCE_DIR}/src/sha256.c
)

target_include_directories(${PROJECT_NAME}-python-wrapper PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Python_INCLUDE_DIRS}
    ${UUID_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}-python-wrapper
    ${Python_LIBRARIES}
    ${UUID_LIBRARY}
    ${PROJECT_NAME}
)

set_target_properties(${PROJECT_NAME}-python-wrapper
    PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME}-python-wrapper
    PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

target_compile_options(${PROJECT_NAME}-python-wrapper PRIVATE -Wextra -pedantic)

set_target_properties(${PROJECT_NAME}-python-wrapper PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME}-python-wrapper
    PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME}-python-wrapper
    LIBRARY DESTINATION ${Python_SITELIB}
)
