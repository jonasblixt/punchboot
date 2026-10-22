# Compatibility guards (D-008, doc/dev/compatibility.md).
#
# abi_dump prints the frozen layout of the wire protocol and the bpak format,
# and check_abi.py compares it with abi.txt, which may only grow.

add_executable(abi_dump abi_dump.c)
target_link_libraries(abi_dump PRIVATE pb_host_flags)

add_test(
    NAME compat.abi
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_abi.py
            check $<TARGET_FILE:abi_dump> ${CMAKE_CURRENT_SOURCE_DIR}/abi.txt
)
set_tests_properties(compat.abi PROPERTIES LABELS compat TIMEOUT 60)
