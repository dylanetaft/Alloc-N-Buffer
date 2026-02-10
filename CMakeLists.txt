cmake_minimum_required(VERSION 3.14)
project(allocnbuffer VERSION 1.0.0 LANGUAGES C)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

file(GLOB_RECURSE SOURCE_FILES ${PROJECT_SOURCE_DIR}/src/*.c)

option(BUILD_TESTS "Build tests" OFF)
option(BUILD_FUZZ "Build fuzz targets" OFF)
option(BUILD_STATIC "Build static library" ON)
option(BUILD_SHARED "Build shared library" OFF)

# Object library — compiled once, reused by static and shared targets
add_library(${PROJECT_NAME}_obj OBJECT ${SOURCE_FILES})
target_include_directories(${PROJECT_NAME}_obj PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/allocnbuffer>
)
set_target_properties(${PROJECT_NAME}_obj PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(_install_targets)

# Static library
if(BUILD_STATIC)
    add_library(${PROJECT_NAME}_static STATIC $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
    target_include_directories(${PROJECT_NAME}_static PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/allocnbuffer>
    )
    set_target_properties(${PROJECT_NAME}_static PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        EXPORT_NAME static
    )
    add_library(allocnbuffer::static ALIAS ${PROJECT_NAME}_static)
    list(APPEND _install_targets ${PROJECT_NAME}_static)
endif()

# Shared library
if(BUILD_SHARED)
    add_library(${PROJECT_NAME}_shared SHARED $<TARGET_OBJECTS:${PROJECT_NAME}_obj>)
    target_include_directories(${PROJECT_NAME}_shared PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/allocnbuffer>
    )
    set_target_properties(${PROJECT_NAME}_shared PROPERTIES
        OUTPUT_NAME ${PROJECT_NAME}
        EXPORT_NAME shared
    )
    add_library(allocnbuffer::shared ALIAS ${PROJECT_NAME}_shared)
    list(APPEND _install_targets ${PROJECT_NAME}_shared)
endif()

# Install targets, headers, and CMake package config
if(_install_targets)
    install(TARGETS ${_install_targets}
        EXPORT allocnbufferTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(EXPORT allocnbufferTargets
        FILE allocnbufferTargets.cmake
        NAMESPACE allocnbuffer::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocnbuffer
    )

    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/allocnbufferConfig.cmake.in
        ${PROJECT_BINARY_DIR}/allocnbufferConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocnbuffer
    )

    write_basic_package_version_file(
        ${PROJECT_BINARY_DIR}/allocnbufferConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${PROJECT_BINARY_DIR}/allocnbufferConfig.cmake
        ${PROJECT_BINARY_DIR}/allocnbufferConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/allocnbuffer
    )
endif()

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/allocnbuffer)


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
    target_link_libraries(anb_tests unity allocnbuffer_static)

    add_test(NAME anb_test COMMAND anb_tests)
endif()

# Fuzzing
option(BUILD_FUZZ "Build fuzz targets (requires clang with libFuzzer)" OFF)

if(BUILD_FUZZ)
    add_executable(fuzz_fifoslab fuzz/fuzz_fifoslab.c)
    target_link_libraries(fuzz_fifoslab allocnbuffer_static)
    target_compile_options(fuzz_fifoslab PRIVATE -fsanitize=fuzzer,address,undefined -g -O1)
    target_link_options(fuzz_fifoslab PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
