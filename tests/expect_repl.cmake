if(NOT DEFINED NOVA OR NOT DEFINED INPUT OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "NOVA, INPUT and EXPECTED are required")
endif()

if(NOT DEFINED EXPECTED_ERRORS)
    set(EXPECTED_ERRORS ${CMAKE_CURRENT_BINARY_DIR}/repl_actual_errors.txt)

    file(WRITE ${EXPECTED_ERRORS} "")
endif()

execute_process(
    COMMAND ${NOVA} repl
    INPUT_FILE ${INPUT}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
)

if(NOT status EQUAL 0)
    message(FATAL_ERROR "the prompt failed with status ${status}\n${errors}")
endif()

set(actual ${CMAKE_CURRENT_BINARY_DIR}/repl_actual.txt)
set(actual_errors ${CMAKE_CURRENT_BINARY_DIR}/repl_actual_errors.txt)

file(WRITE ${actual} "${output}")
file(WRITE ${actual_errors} "${errors}")

file(MD5 ${actual} actual_hash)
file(MD5 ${EXPECTED} expected_hash)

if(NOT actual_hash STREQUAL expected_hash)
    file(READ ${EXPECTED} expected)

    message(FATAL_ERROR "output mismatch\n--- expected ---\n${expected}\n--- actual ---\n${output}\n--- end ---")
endif()

file(MD5 ${actual_errors} errors_hash)
file(MD5 ${EXPECTED_ERRORS} expected_errors_hash)

if(NOT errors_hash STREQUAL expected_errors_hash)
    file(READ ${EXPECTED_ERRORS} expected)

    message(FATAL_ERROR "error output mismatch\n--- expected ---\n${expected}\n--- actual ---\n${errors}\n--- end ---")
endif()