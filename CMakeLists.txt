cmake_minimum_required(VERSION 3.20)
project(netopt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler Warnings Configuration
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Sanitizers Option (ASan + UBSan)
option(ENABLE_SANITIZERS "Enable Address and Undefined Behavior Sanitizers" OFF)
if(ENABLE_SANITIZERS)
    if(MSVC)
        add_compile_options(/fsanitize=address)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Core Library Sources
set(NETOPT_CORE_SOURCES
    algo/kruskal.cpp
    algo/prim.cpp
    algo/prim_dense.cpp
    algo/mst_algorithm.cpp
    io/csv_tokenizer.cpp
    io/row_validator.cpp
    io/graph_repository.cpp
    analysis/verifier.cpp
    analysis/sensitivity.cpp
    analysis/benchmark.cpp
    export/dot_exporter.cpp
    export/dot_frame_sink.cpp
    ui/menu.cpp
)

add_library(netopt_core STATIC ${NETOPT_CORE_SOURCES})
target_include_directories(netopt_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Main Application Executable
add_executable(netopt main.cpp)
target_link_libraries(netopt PRIVATE netopt_core)

# Testing
enable_testing()

set(NETOPT_TEST_SOURCES
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_dsu.cpp
    tests/test_heap.cpp
    tests/test_kruskal.cpp
    tests/test_prim.cpp
    tests/test_parser.cpp
    tests/test_verifier.cpp
    tests/test_sensitivity.cpp
    tests/test_crossval.cpp
)

add_executable(netopt_tests ${NETOPT_TEST_SOURCES})
target_include_directories(netopt_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_link_libraries(netopt_tests PRIVATE netopt_core)

add_test(NAME NetoptTests COMMAND netopt_tests)

# Benchmark Executable
add_executable(netopt_benchmark tests/benchmark_runner.cpp)
target_include_directories(netopt_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(netopt_benchmark PRIVATE netopt_core)
