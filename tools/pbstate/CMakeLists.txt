cmake_minimum_required(VERSION 3.10)

project(pbstate
    VERSION 0.3.2
    DESCRIPTION "pbstate"
    HOMEPAGE_URL https://github.com/jonasblixt/punchboot
)

enable_language(C)

set(CMAKE_C_STANDARD 99)

add_compile_options(-Wall -Werror -Wextra -pedantic)

include(GNUInstallDirs)

# @ONLY "Restrict variable replacement to references of the form @VAR@"
configure_file(src/pbstate.pc.in pbstate.pc @ONLY)

set(LIB_SRC_FILES
    src/crc.c
    src/pbstate.c
)

set(LIB_PUBLIC_HEADERS
    src/pbstate.h
)

SET(TOOL_SRC_FILES
    src/main.c
)

set(TOOL_CFLAGS
    -DPROJECT_VERSION="${PROJECT_VERSION}"
)

set(TOOL_LDFLAGS
    ${PROJECT_NAME}
)

add_executable(${PROJECT_NAME}-tool ${TOOL_SRC_FILES})
target_compile_options(${PROJECT_NAME}-tool PRIVATE ${TOOL_CFLAGS})
target_link_libraries(${PROJECT_NAME}-tool ${TOOL_LDFLAGS})

set_target_properties(${PROJECT_NAME}-tool
    PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

add_library(
    ${PROJECT_NAME} SHARED ${LIB_SRC_FILES}
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pbstate
)

install(TARGETS ${PROJECT_NAME}-tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
