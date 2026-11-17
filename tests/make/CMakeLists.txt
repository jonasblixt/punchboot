# The firmware build (the Makefile): what it makes again when an input
# changes. It runs make with the firmware's toolchain, and is skipped without
# one (outside the dev container).

add_test(
    NAME make.rebuild
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_rebuild.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/rebuild
)
set_tests_properties(make.rebuild PROPERTIES LABELS make TIMEOUT 120 SKIP_RETURN_CODE 77)

# The keystore's generator (board.md 7): needs no toolchain
add_test(
    NAME make.gen_keystore
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_gen_keystore.py
            ${CMAKE_SOURCE_DIR}
)
set_tests_properties(make.gen_keystore PROPERTIES LABELS make TIMEOUT 60)

add_test(
    NAME make.config
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_config.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/config
)
set_tests_properties(make.config PROPERTIES LABELS make TIMEOUT 60 SKIP_RETURN_CODE 77)

# The link's checks of a board's addresses (platform.md 5.1): each fails when
# its condition is broken. It builds a board of each platform
add_test(
    NAME make.link
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_link.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/link
)
set_tests_properties(make.link PROPERTIES LABELS make TIMEOUT 300 SKIP_RETURN_CODE 77)

add_test(
    NAME make.include_dirs
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_include_dirs.py
            ${CMAKE_SOURCE_DIR}
)
set_tests_properties(make.include_dirs PROPERTIES LABELS make TIMEOUT 60)

# The host build: pb-host links every file of the firmware core, as the
# firmware's link does, so a task in a file nothing refers to runs (F-407)
add_test(
    NAME make.host_link
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_host_link.py
            ${CMAKE_NM} $<TARGET_FILE:pb-host> $<TARGET_OBJECTS:pb_core>
)
set_tests_properties(make.host_link PROPERTIES LABELS make TIMEOUT 60)

# The image stages as a board out of the tree gets them (platform.md 6.2):
# image-y, FINAL_OUTPUT and the levels at which a board overrides
add_test(
    NAME make.stages
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_stages.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/stages
)
set_tests_properties(make.stages PROPERTIES LABELS make TIMEOUT 300 SKIP_RETURN_CODE 77)
