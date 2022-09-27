include(GNUInstallDirs)

# @ONLY "Restrict variable replacement to references of the form @VAR@"
configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)


set(LIB_SRC_FILES
    api.c
    api_authentication.c
    api_board.c
    api_boot.c
    api_device.c
    api_misc.c
    api_partition.c
    api_slc.c
    api_stream.c
    error.c
    usb.c
    wire.c
)

if (PB_TOOLS_BUILD_SOCKET)
    set(LIB_SRC_FILES
        ${LIB_SRC_FILES}
        socket.c
    )
endif()

# Every .h -file in include/pb-tools/ is a public header
file(GLOB_RECURSE LIB_PUBLIC_HEADERS "${CMAKE_SOURCE_DIR}/include/pb-tools/*.h")

SET(CMAKE_C_VISIBILITY_PRESET hidden)

add_library(
    ${PROJECT_NAME} SHARED ${LIB_SRC_FILES}
)

add_library(
    ${PROJECT_NAME}-static STATIC ${LIB_SRC_FILES}
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}"
)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION ${PROJECT_VERSION_MAJOR})
set_target_properties(${PROJECT_NAME}-static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

set(LIB_CFLAGS
    -Wextra -pedantic
    ${LIBUSB_CFLAGS}
    ${BPAK_CFLAGS}
)

set(LIB_LDFLAGS
    ${LIBUSB_LDFLAGS}
    ${BPAK_LDFLAGS}
)

target_compile_options(${PROJECT_NAME} PRIVATE  ${LIB_CFLAGS} )
target_compile_options(${PROJECT_NAME}-static PRIVATE ${LIB_CFLAGS})
target_link_libraries(${PROJECT_NAME} ${LIB_LDFLAGS})
target_link_libraries(${PROJECT_NAME}-static ${LIB_LDFLAGS})

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)

install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}-static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pb-tools
)
