find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(E2E_TIMEOUT_SECONDS 300)

function(add_e2e_tests group)
    foreach(impl ${ARGN})
        add_test(NAME e2e_${impl}_${group}
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/cli_e2e.py
                --binary $<TARGET_FILE:rank_queries>
                --impl ${impl}
                --group ${group})
        set_tests_properties(e2e_${impl}_${group} PROPERTIES LABELS "e2e" TIMEOUT ${E2E_TIMEOUT_SECONDS})
    endforeach()
endfunction()

foreach(group basic edge input)
    add_e2e_tests(${group} vector set avl btree)
endforeach()
