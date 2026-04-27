cmake_minimum_required(VERSION 3.20)
project(CBALT VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include_directories(${CMAKE_SOURCE_DIR}/include)

# Source files
set(CBALT_SOURCES
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/codegen.cpp
)

# Main compiler executable
add_executable(cbalt ${CBALT_SOURCES})

# Platform-specific linker flags
if(WIN32)
    set_target_properties(cbalt PROPERTIES OUTPUT_NAME "cbalt")
elseif(UNIX)
    target_link_libraries(cbalt pthread)
endif()

# Tests
enable_testing()

set(TEST_DRIVER ${CMAKE_SOURCE_DIR}/tests/test_main.cpp)
if(EXISTS ${TEST_DRIVER})
    add_executable(cbalt_tests
        ${TEST_DRIVER}
        src/lexer.cpp
        src/parser.cpp
        src/codegen.cpp
    )
    if(UNIX)
        target_link_libraries(cbalt_tests pthread)
    endif()

    add_test(NAME SmokeTests COMMAND cbalt_tests)
endif()

# Print build info
message(STATUS "CBALT v${PROJECT_VERSION} — Build configured.")
message(STATUS "Compiler : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Output   : ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
