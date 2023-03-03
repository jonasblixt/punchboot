
add_compile_options(
        -fsanitize=address
        -DTEST_SRC_DIR="${CMAKE_SOURCE_DIR}/test"
)
add_link_options(-fsanitize=address)
include_directories(${CMAKE_SOURCE_DIR}/lib)

set(C_TESTS_COMMON
    command.c
    common.c
    nala.c
    test_command_loop.c
    test_command_parser.c
    ../src/uuid/clear.c
    ../src/uuid/compare.c
    ../src/uuid/copy.c
    ../src/uuid/isnull.c
    ../src/uuid/pack.c
    ../src/uuid/parse.c
    ../src/uuid/unpack.c
    ../src/uuid/unparse.c
)

set(C_TESTS
    test_api
    test_api_partition
    test_authentication
    test_board
    test_boot
    test_bootloader_version
    test_ctx
    test_error_codes
    test_help
    test_magic
    test_slc
    test_stream
    test_wire_format
)

foreach(c_test IN LISTS C_TESTS)
    add_executable(${c_test} ${c_test}.c ${C_TESTS_COMMON})
    target_link_libraries(${c_test}
        ${PROJECT_NAME}
        ${BPAK_LDFLAGS}
        ${UUID_LDFLAGS}
    )
    target_compile_options(${c_test} PRIVATE
        ${UUID_CFLAGS}
        -std=c11
    )
    add_test(${c_test} ${c_test})
endforeach()

