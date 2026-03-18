cmake_minimum_required(VERSION 3.20)
project(viam-camera-ensenso VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Check if building with Conan
if(EXISTS "${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
    message(STATUS "Building with Conan dependencies")
    include("${CMAKE_BINARY_DIR}/conan_toolchain.cmake")
endif()

# Find Viam SDK (optional for now - can build test without it)
find_package(viam-cpp-sdk QUIET)
if(viam-cpp-sdk_FOUND)
    set(BUILD_VIAM_MODULE ON)
    message(STATUS "Found Viam C++ SDK - will build full module")
else()
    set(BUILD_VIAM_MODULE OFF)
    message(STATUS "Viam C++ SDK not found - will only build test utilities")
    message(STATUS "To build the full module, run: make conan-install")
endif()

# Ensenso SDK
set(ENSENSO_SDK_DIR "/opt/ensenso" CACHE PATH "Path to Ensenso SDK installation")
set(ENSENSO_INCLUDE_DIR "${ENSENSO_SDK_DIR}/development/c/include")
set(ENSENSO_LIB_DIR "${ENSENSO_SDK_DIR}/lib")

# Check if Ensenso SDK exists
if(NOT EXISTS "${ENSENSO_INCLUDE_DIR}/nxLib.h")
    message(FATAL_ERROR "Ensenso SDK not found at ${ENSENSO_SDK_DIR}. Please install the SDK or set ENSENSO_SDK_DIR.")
endif()

# Find Ensenso library
find_library(ENSENSO_LIBRARY
    NAMES NxLib64 NxLib
    PATHS "${ENSENSO_LIB_DIR}"
    NO_DEFAULT_PATH
    REQUIRED
)

if(NOT ENSENSO_LIBRARY)
    message(FATAL_ERROR "Ensenso library (libNxLib64.so) not found in ${ENSENSO_LIB_DIR}")
endif()

message(STATUS "Found Ensenso SDK at: ${ENSENSO_SDK_DIR}")
message(STATUS "Ensenso include dir: ${ENSENSO_INCLUDE_DIR}")
message(STATUS "Ensenso library: ${ENSENSO_LIBRARY}")

# Only build the full Viam module if SDK is available
if(BUILD_VIAM_MODULE)
    # Source files
    set(SOURCES
        src/module/main.cpp
        src/module/ensenso_camera.cpp
    )

    set(HEADERS
        src/module/ensenso_camera.hpp
    )

    # Create executable
    add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

    # Include directories
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/module
        ${ENSENSO_INCLUDE_DIR}
    )

    # Link libraries
    target_link_libraries(${PROJECT_NAME} PRIVATE
        viam-cpp-sdk::viamsdk
        ${ENSENSO_LIBRARY}
    )

    # Set RPATH for finding shared libraries at runtime
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "${ENSENSO_LIB_DIR}"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    # Installation
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
    )

    # Install metadata
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/etc/meta.json
        DESTINATION etc
    )
endif()

# Test executable (optional - for testing SDK integration)
add_executable(test-ensenso-sdk
    src/test/test_ensenso_sdk.cpp
)

target_include_directories(test-ensenso-sdk PRIVATE
    ${ENSENSO_INCLUDE_DIR}
)

target_link_libraries(test-ensenso-sdk PRIVATE
    ${ENSENSO_LIBRARY}
)

set_target_properties(test-ensenso-sdk PROPERTIES
    INSTALL_RPATH "${ENSENSO_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
)
