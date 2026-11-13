# Integration tests: pytest drives pb-host through the punchboot Python
# package, which must be installed in the Python environment running CTest
# (pip install .).

add_test(
    NAME integration
    COMMAND ${Python3_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR} -q
            -p no:cacheprovider
            --basetemp=${CMAKE_CURRENT_BINARY_DIR}/tmp
)
# The suite takes ~10 s (longer with sanitizers or coverage); the timeout
# stops a hung pb-host or test from blocking CI
set_tests_properties(integration PROPERTIES
    LABELS integration
    TIMEOUT 600
    ENVIRONMENT PB_HOST_BIN=$<TARGET_FILE:pb-host>
)
# scripts/ci/host-test.sh builds pbstate with the sanitizers too. One test
# preloads zlib before it and one loads libpbstate into Python: ASan refuses
# both, its runtime isn't the first library there
if(PB_SANITIZE)
    set_property(TEST integration APPEND PROPERTY
        ENVIRONMENT ASAN_OPTIONS=verify_asan_link_order=0)
endif()
