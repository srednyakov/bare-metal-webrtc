cmake_minimum_required(VERSION 3.20)

project(win-capture-native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg triplet")

# ---- Profiling (Tracy) ----
option(ENABLE_PROFILING "Enable Tracy profiling" OFF)

if(ENABLE_PROFILING)
    add_compile_definitions(TRACY_ENABLE)
    message(STATUS "Tracy profiling enabled")
endif()

# -------- DLL --------
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/capture.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/capturer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/capturer_fabric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/encoders/encoder_x264.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/internal/buffers/captured_buffer.cpp
)

set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/capture.h
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
    # DLL
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"

    # .lib (import / static)
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib/RelWithDebInfo"
    ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib/MinSizeRel"

    LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/lib/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib/Release"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/lib/RelWithDebInfo"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/lib/MinSizeRel"
)

# ---- vcpkg / x264 ----
if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR
        "${CMAKE_SOURCE_DIR}/third-party/vcpkg/installed"
        CACHE PATH "vcpkg installed dir")
endif()

set(X264_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
message(STATUS "Using x264 from: ${X264_ROOT}")

find_package(yaml-cpp CONFIG REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${X264_ROOT}/include"
)

target_link_directories(${PROJECT_NAME} PRIVATE
    "${X264_ROOT}/lib"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    libx264
    dxgi
    d3d11
    yaml-cpp::yaml-cpp
)

set(TRACY_ON_DEMAND ON CACHE BOOL "" FORCE) # Don't wait for Tracy client connection (non-blocking)
set(TRACY_NO_SYSTEM_TRACING ON CACHE BOOL "" FORCE) # Disable ETW (Windows Event Tracing) to avoid handle macro conflict
set(TRACY_NO_BROADCAST ON CACHE BOOL "" FORCE) # Disable broadcast (network discovery)
set(TRACY_NO_CRASH_HANDLER ON CACHE BOOL "" FORCE) # Explicitly disable crash handler

# ---- Add Tracy Profiler ----
if(ENABLE_PROFILING)
    add_subdirectory(third-party/tracy)
    target_link_libraries(${PROJECT_NAME} PRIVATE TracyClient)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/third-party/tracy/public
)

add_subdirectory(tests)
