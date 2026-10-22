# C unit tests (CMocka). Each test links the modules under test, the shared
# libraries and the fakes it needs.

find_package(PkgConfig REQUIRED)
pkg_check_modules(CMOCKA REQUIRED IMPORTED_TARGET cmocka)

add_library(pb_unit_fakes STATIC
    fakes/plat.c
    fakes/ramdisk.c
)
target_link_libraries(pb_unit_fakes PUBLIC pb_lib)

function(pb_unit_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES" ${ARGN})
    add_executable(${name} ${name}.c ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE pb_unit_fakes PkgConfig::CMOCKA)
    add_test(NAME unit.${name} COMMAND ${name})
    set_tests_properties(unit.${name} PROPERTIES LABELS unit)
endfunction()

pb_unit_test(test_bio SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
)

pb_unit_test(test_gpt SOURCES
    ${PROJECT_SOURCE_DIR}/src/bio.c
    ${PROJECT_SOURCE_DIR}/src/drivers/partition/gpt.c
)
