
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
)

add_executable(${PROJECT_NAME}-tool ${TOOL_SRC_FILES})
set_target_properties(${PROJECT_NAME}-tool
    PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

target_compile_options(${PROJECT_NAME}-tool
    PRIVATE -Wextra -pedantic ${UUID_CFLAGS} ${BPAK_CFLAGS} ${UUID_CFLAGS}
)

target_link_libraries(${PROJECT_NAME}-tool
    ${UUID_LDFLAGS}
    ${BPAK_LDFLAGS}
    ${UUID_LDFLAGS}
    ${PROJECT_NAME}
)

install(TARGETS ${PROJECT_NAME}-tool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
