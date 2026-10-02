cmake_minimum_required(VERSION 3.16)

project(nova VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

include(GNUInstallDirs)

set(NOVA_SOURCES
    src/errors/errors.cpp

    src/lexer/lexer.cpp

    src/parser/parser.cpp
    src/parser/ast.cpp

    src/compiler/opcode.cpp
    src/compiler/bytecode.cpp
    src/compiler/compiler.cpp

    src/runtime/value.cpp
    src/runtime/object.cpp
    src/runtime/object_function.cpp
    src/runtime/object_native.cpp
    src/runtime/object_list.cpp
    src/runtime/object_map.cpp
    src/runtime/object_class.cpp
    src/runtime/object_module.cpp
    src/runtime/upvalue.cpp

    src/vm/stack.cpp
    src/vm/vm.cpp

    src/io/file_io.cpp
    src/net/net.cpp

    src/stdlib/stdlib.cpp

    src/debug/disassembler.cpp
)

add_library(nova_core STATIC ${NOVA_SOURCES})

target_include_directories(nova_core PUBLIC src)

target_compile_definitions(
    nova_core
    PUBLIC NOVA_VERSION="${PROJECT_VERSION}"
)

find_package(Threads REQUIRED)

target_link_libraries(nova_core PUBLIC Threads::Threads)

add_executable(nova src/main.cpp)

target_link_libraries(nova PRIVATE nova_core)

add_executable(nova_diagnostics tests/diagnostics.cpp)

target_link_libraries(nova_diagnostics PRIVATE nova_core)

add_executable(nova_lexer_tests tests/lexer_tests.cpp)

target_link_libraries(nova_lexer_tests PRIVATE nova_core)

add_executable(nova_parser_tests tests/parser_tests.cpp)

target_link_libraries(nova_parser_tests PRIVATE nova_core)

add_executable(nova_vm_tests tests/vm_tests.cpp)

target_link_libraries(nova_vm_tests PRIVATE nova_core)

enable_testing()

add_test(
    NAME language
    COMMAND nova run ${CMAKE_CURRENT_SOURCE_DIR}/tests/language.nova
)

add_test(
    NAME diagnostics
    COMMAND nova_diagnostics
)

add_test(
    NAME lexer
    COMMAND nova_lexer_tests
)

add_test(
    NAME parser
    COMMAND nova_parser_tests
)

add_test(
    NAME vm
    COMMAND nova_vm_tests
)

add_test(
    NAME repl
    COMMAND ${CMAKE_COMMAND}
        -DNOVA=$<TARGET_FILE:nova>
        -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_input.txt
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_output.txt
        -DEXPECTED_ERRORS=${CMAKE_CURRENT_SOURCE_DIR}/tests/repl_error.txt
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_repl.cmake
)

add_test(
    NAME example_modules
    COMMAND ${CMAKE_COMMAND}
        -DNOVA=$<TARGET_FILE:nova>
        -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/modules/main.nova
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/tests/modules/main.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_output.cmake
)

add_executable(nova_embed examples/embed.cpp)
target_link_libraries(nova_embed PRIVATE nova_core)

add_test(
    NAME example_embed
    COMMAND ${CMAKE_COMMAND}
        -DNOVA=$<TARGET_FILE:nova_embed>
        -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/examples/embed.out
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_program.cmake
)

file(GLOB NOVA_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/programs/*.nova)

foreach(example ${NOVA_EXAMPLES})
    get_filename_component(name ${example} NAME_WE)

    add_test(
        NAME example_${name}
        COMMAND ${CMAKE_COMMAND}
            -DNOVA=$<TARGET_FILE:nova>
            -DSOURCE=${example}
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/programs/${name}.out
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_output.cmake
    )
endforeach()

install(TARGETS nova RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS nova_core ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY src/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nova FILES_MATCHING PATTERN "*.h")
