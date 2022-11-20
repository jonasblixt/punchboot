
SET(TOOL_SRC_FILES
    auth.c
    board.c
    boot.c
    crc.c
    dev.c
    help.c
    main.c
    part.c
    sha256.c
    slc.c
    utils.c
    uuid/clear.c
    uuid/compare.c
    uuid/copy.c
    uuid/isnull.c
    uuid/pack.c
    uuid/parse.c
    uuid/unpack.c
    uuid/unparse.c
)

add_executable(${PROJECT_NAME}-tool ${TOOL_SRC_FILES})
set_target_properties(${PROJECT_NAME}-tool
    PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

set(TOOL_CFLAGS
    -Wextra -pedantic
    ${BPAK_CFLAGS}
    ${LIBUSB_CFLAGS}
)

if (PB_TOOLS_BUILD_SOCKET)
set(TOOL_CFLAGS
    ${TOOL_CFLAGS}
    -DPB_TOOLS_BUILD_SOCKET=1
)
endif()

target_compile_options(${PROJECT_NAME}-tool PRIVATE ${TOOL_CFLAGS})

set(TOOL_LDFLAGS
    ${BPAK_LDFLAGS}
    ${LIBUSB_LDFLAGS}
)

if (PB_TOOLS_BUILD_STATIC)
    set(TOOL_LDFLAGS
        --static
        ${PROJECT_NAME}-static
        ${TOOL_LDFLAGS}
    )
else()
    set(TOOL_LDFLAGS
        ${PROJECT_NAME}
        ${TOOL_LDFLAGS}
    )
endif()

target_link_libraries(${PROJECT_NAME}-tool ${TOOL_LDFLAGS})

install(TARGETS ${PROJECT_NAME}-tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
