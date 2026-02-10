cmake_minimum_required(VERSION 3.14)
project(alloc-n-buffer C)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_FUZZ "Build fuzz targets" OFF)

# Object library — compiled once, reused by static and shared targets
add_library(${PROJECT_NAME}_obj OBJECT ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME}_obj PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(${PROJECT_NAME}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Static library
add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
target_include_directories(${PROJECT_NAME}_static PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(${PROJECT_NAME}_static PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

# Shared library
add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
target_include_directories(${PROJECT_NAME}_shared PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(${PROJECT_NAME}_shared PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

# Install targets and headers
install(TARGETS ${PROJECT_NAME}_static ${PROJECT_NAME}_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include/allocnbuffer)


if(BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        unity
        GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
        GIT_TAG        v2.6.1
    )
    FetchContent_MakeAvailable(unity)

    add_executable(anb_tests tests/test_fifoslab.c)
    target_link_libraries(anb_tests unity alloc-n-buffer_static)

    add_test(NAME anb_test COMMAND anb_tests)
endif()

# Fuzzing
option(BUILD_FUZZ "Build fuzz targets (requires clang with libFuzzer)" OFF)

if(BUILD_FUZZ)
    add_executable(fuzz_fifoslab fuzz/fuzz_fifoslab.c)
    target_link_libraries(fuzz_fifoslab alloc-n-buffer_static)
    target_compile_options(fuzz_fifoslab PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(fuzz_fifoslab PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
