cmake_minimum_required(VERSION 3.25)

project(win-capture-native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_link_options(
    -static-libgcc
    -static-libstdc++
)

option(ENABLE_PROFILING "Enable Tracy profiling" OFF)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/errors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/capturer_fabric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/encoders/encoder_x264.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/buffers/captured_buffer.cpp
)

set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/capture.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/errors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/types.h
)

add_library(${PROJECT_NAME} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE CAPTURE_NATIVE_BUILD)

target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/internal>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third-party/SPSCQueue/include>
    $<INSTALL_INTERFACE:include>
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"

    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib/RelWithDebInfo"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib/MinSizeRel"

    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib/RelWithDebInfo"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib/MinSizeRel"
)

if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR
        "${CMAKE_SOURCE_DIR}/third-party/vcpkg/installed"
        CACHE PATH "vcpkg installed dir")
endif()

find_library(X264_LIB
    NAMES libx264.a
    PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib"
    NO_DEFAULT_PATH
    REQUIRED
)

message(STATUS "x264: ${X264_LIB}")

find_package(yaml-cpp CONFIG REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${X264_ROOT}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${X264_LIB}
    yaml-cpp::yaml-cpp
    dxgi
    d3d11
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${X264_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/third-party/tracy/public
)


set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE)
set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "" FORCE)
set(TRACY_NO_BROADCAST ON CACHE BOOL "" FORCE)
set(TRACY_NO_CRASH_HANDLER ON CACHE BOOL "" FORCE)
set(TRACY_NO_COMPRESS ON CACHE BOOL "" FORCE)

if(ENABLE_PROFILING)
    add_compile_definitions(TRACY_ENABLE)
    add_subdirectory(third-party/tracy)
    target_link_libraries(${PROJECT_NAME} PRIVATE TracyClient)
    message(STATUS "Tracy profiling enabled")
endif()

add_subdirectory(tests)
