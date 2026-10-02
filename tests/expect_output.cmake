if(NOT DEFINED NOVA OR NOT DEFINED SOURCE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "NOVA, SOURCE and EXPECTED are required")
endif()

execute_process(
    COMMAND ${NOVA} run ${SOURCE}
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    RESULT_VARIABLE status
)

if(NOT status EQUAL 0)
    message(FATAL_ERROR "program failed with status ${status}\n${errors}")
endif()

set(actual ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CURRENT_LIST_FILE}.actual)

file(WRITE ${actual} "${output}")

file(MD5 ${actual} actual_hash)
file(MD5 ${EXPECTED} expected_hash)

if(NOT actual_hash STREQUAL expected_hash)
    file(READ ${EXPECTED} expected)

    message(FATAL_ERROR "output mismatch for ${SOURCE}\n--- expected ---\n${expected}\n--- actual ---\n${output}\n--- end ---")
endif()