# Integration tests: pytest drives pb-host through the punchboot Python
# package, which must be installed in the Python environment running CTest
# (pip install .).

add_test(
    NAME integration
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR} -q
            -p no:cacheprovider
            --basetemp=${CMAKE_CURRENT_BINARY_DIR}/tmp
)
set_tests_properties(integration PROPERTIES
    LABELS integration
    ENVIRONMENT PB_HOST_BIN=$<TARGET_FILE:pb-host>
)
