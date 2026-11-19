cmake_minimum_required(VERSION 3.16)
project(roaring_bitmaps_eda LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)
if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    else()
        message(FATAL_ERROR "Sanitizers require GCC or Clang")
    endif()
endif()

add_library(roaring_bitmap src/roaring_bitmap.cpp)
target_include_directories(roaring_bitmap PUBLIC include)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(roaring_bitmap PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_roaring_bitmap tests/test_roaring_bitmap.cpp)
    target_link_libraries(test_roaring_bitmap PRIVATE roaring_bitmap)
    add_test(NAME roaring_bitmap COMMAND test_roaring_bitmap)
endif()

add_library(retail_index src/retail_index.cpp)
target_link_libraries(retail_index PUBLIC roaring_bitmap)

add_executable(retail_cli apps/retail_cli.cpp)
target_link_libraries(retail_cli PRIVATE retail_index)

add_executable(retail_benchmark benchmarks/retail_benchmark.cpp)
target_link_libraries(retail_benchmark PRIVATE retail_index)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    foreach(target retail_index retail_cli retail_benchmark)
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endforeach()
endif()

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_executable(test_retail_index tests/test_retail_index.cpp)
    target_link_libraries(test_retail_index PRIVATE retail_index)
    add_test(NAME retail_index COMMAND test_retail_index)
    add_test(NAME prepare_data COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_prepare_data.py)
    add_test(NAME retail_cli_smoke COMMAND retail_cli
        --data ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/retail.csv
        --type term --first red --limit 3 --verify)
    add_test(NAME retail_cli_invalid COMMAND retail_cli
        --data ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/retail.csv
        --type and --first red)
    set_tests_properties(retail_cli_invalid PROPERTIES WILL_FAIL TRUE)
    add_test(NAME benchmark_smoke COMMAND retail_benchmark
        --data ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/retail.csv
        --output ${CMAKE_CURRENT_BINARY_DIR}/benchmark_smoke
        --repetitions 2 --warmup 1 --build-repetitions 2 --overwrite)
endif()

if(BUILD_TESTING)
    add_test(NAME experiment_evidence COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/test_experiments.py $<TARGET_FILE:retail_benchmark>)
endif()
