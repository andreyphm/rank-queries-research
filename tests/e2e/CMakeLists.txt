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

foreach(group basic edge input scale_mixed)
    add_e2e_tests(${group} vector set avl btree)
endforeach()

add_e2e_tests(scale_big avl btree)

add_test(NAME e2e_gen
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/gen_e2e.py
        --binary $<TARGET_FILE:rank_queries_gen>
        --solver $<TARGET_FILE:rank_queries>)
set_tests_properties(e2e_gen PROPERTIES LABELS "e2e" TIMEOUT ${E2E_TIMEOUT_SECONDS})

add_test(NAME e2e_bench
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/bench_e2e.py
        --binary $<TARGET_FILE:rank_queries_bench>
        --solver $<TARGET_FILE:rank_queries>
        --generator $<TARGET_FILE:rank_queries_gen>)
set_tests_properties(e2e_bench PROPERTIES LABELS "e2e" TIMEOUT ${E2E_TIMEOUT_SECONDS})
