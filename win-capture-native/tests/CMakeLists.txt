add_executable(${PROJECT_NAME}-smoke-test
    smoke_test.cpp
    ${SOURCES}
)

set_target_properties(${PROJECT_NAME}-smoke-test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_BINARY_DIR}/bin/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin/Release"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin/RelWithDebInfo"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin/MinSizeRel"
)

target_link_directories(${PROJECT_NAME}-smoke-test PRIVATE
    "${X264_ROOT}/lib"
)

target_link_libraries(${PROJECT_NAME}-smoke-test PRIVATE
    libx264
    dxgi
    d3d11
    yaml-cpp::yaml-cpp
)

if(ENABLE_PROFILING)
    message(STATUS "Tracy profiling enabled for smoke-test")
    target_link_libraries(${PROJECT_NAME}-smoke-test PRIVATE TracyClient)
    target_compile_definitions(${PROJECT_NAME}-smoke-test PRIVATE TRACY_ENABLE)
endif()

target_compile_definitions(${PROJECT_NAME}-smoke-test PRIVATE CAPTURE_NATIVE_BUILD)

target_include_directories(${PROJECT_NAME}-smoke-test PUBLIC
    "${CMAKE_SOURCE_DIR}/third-party/SPSCQueue/include"
    "${CMAKE_SOURCE_DIR}/include/internal"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/third-party/tracy/public"
)

add_custom_command(TARGET ${PROJECT_NAME}-smoke-test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/tests/smoke_config.yaml"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}-smoke-test>/smoke_config.yaml"
)
