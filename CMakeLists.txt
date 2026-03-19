cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
set(CMAKE_PROJECT_VERSION 0.1.0)

project(viam-camera-ensenso
    DESCRIPTION "Viam Module for IDS Ensenso 3D Cameras"
    HOMEPAGE_URL https://github.com/yourusername/viam-camera-ensenso
    LANGUAGES CXX
)

option(VIAM_ENSENSO_ENABLE_TESTS "Enable unit tests" OFF)

# Set RPATH for runtime library finding
if (APPLE)
    list(PREPEND CMAKE_INSTALL_RPATH "@loader_path/../lib")
else()
    list(PREPEND CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Try to find Viam SDK (may not be available without Conan)
find_package(viam-cpp-sdk QUIET)
if(viam-cpp-sdk_FOUND)
    set(BUILD_VIAM_MODULE ON)
    message(STATUS "Found Viam C++ SDK - will build full module")
else()
    set(BUILD_VIAM_MODULE OFF)
    message(STATUS "Viam C++ SDK not found - will only build test utilities")
    message(STATUS "To build the full module, run: conan install . --build=missing")
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
    # Create executable
    add_executable(${PROJECT_NAME}
        src/module/main.cpp
        src/module/ensenso_camera.cpp
        src/module/nxlib_context.cpp
    )

    # Include directories
    target_include_directories(${PROJECT_NAME}
        PRIVATE src/module
        PRIVATE ${ENSENSO_INCLUDE_DIR}
    )

    # Compile features
    target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

    # Link libraries
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            viam-cpp-sdk::viamsdk
            ${ENSENSO_LIBRARY}
            ${CMAKE_DL_LIBS}
    )

    # Installation (to root for Conan deploy)
    install(TARGETS ${PROJECT_NAME} DESTINATION .)
    install(FILES etc/meta.json DESTINATION .)
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
