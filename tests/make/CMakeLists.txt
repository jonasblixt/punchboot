# The firmware build (the Makefile): what it makes again when an input
# changes. It runs make with the firmware's toolchain, and is skipped without
# one (outside the dev container).

add_test(
    NAME make.rebuild
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_rebuild.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/rebuild
)
set_tests_properties(make.rebuild PROPERTIES LABELS make TIMEOUT 120 SKIP_RETURN_CODE 77)

add_test(
    NAME make.config
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_config.py
            ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/config
)
set_tests_properties(make.config PROPERTIES LABELS make TIMEOUT 60 SKIP_RETURN_CODE 77)

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
