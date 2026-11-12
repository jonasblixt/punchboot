# The document lint (doc/process.md 7): the documents agree with each other
# and with the headers they describe. It reads the source tree, builds nothing.

add_test(
    NAME doc.lint
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_docs.py
            ${CMAKE_SOURCE_DIR}
)
set_tests_properties(doc.lint PROPERTIES LABELS doc TIMEOUT 60)
