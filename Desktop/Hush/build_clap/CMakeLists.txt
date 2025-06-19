cmake_minimum_required(VERSION 3.20)
project(KhDetector_CLAP VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Audio processing options
set(DECIM_FACTOR 3 CACHE STRING "Decimation factor for downsampling (default: 3 for 48kHz->16kHz)")
add_compile_definitions(DECIM_FACTOR=${DECIM_FACTOR})

# Platform-specific settings
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13")
endif()

# Find threading library
find_package(Threads REQUIRED)

# Add CLAP SDK (minimal version without full submodule)
add_library(clap_headers INTERFACE)
target_include_directories(clap_headers INTERFACE
    ../external/clap-sdk/include
)

# CLAP Plugin target
add_library(KhDetector_CLAP MODULE)

# CLAP Plugin sources
target_sources(KhDetector_CLAP PRIVATE
    ../clap/kh_detector_simple.cc
)

# CLAP Include directories
target_include_directories(KhDetector_CLAP PRIVATE
    ../src
    ../external/clap-sdk/include
)

# CLAP Link libraries
target_link_libraries(KhDetector_CLAP PRIVATE
    Threads::Threads
    clap_headers
)

# CLAP Compiler-specific flags
target_compile_features(KhDetector_CLAP PRIVATE cxx_std_17)

# CLAP Platform-specific settings
if(APPLE)
    set_target_properties(KhDetector_CLAP PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "clap"
        MACOSX_BUNDLE_BUNDLE_NAME "KhDetector"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024"
    )
elseif(WIN32)
    set_target_properties(KhDetector_CLAP PROPERTIES
        SUFFIX ".clap"
    )
else()
    set_target_properties(KhDetector_CLAP PROPERTIES
        SUFFIX ".clap"
    )
endif()

# CLAP Compiler settings
if(MSVC)
    target_compile_options(KhDetector_CLAP PRIVATE /W4)
    target_compile_definitions(KhDetector_CLAP PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    # Enable AVX/SSE on Windows
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(KhDetector_CLAP PRIVATE /arch:AVX)
    endif()
else()
    target_compile_options(KhDetector_CLAP PRIVATE -Wall -Wextra -Wpedantic)
    # Enable SIMD optimizations
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(KhDetector_CLAP PRIVATE -msse2 -msse4.1 -mavx)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64" OR CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        # Use default ARM settings for macOS, don't specify march
        target_compile_definitions(KhDetector_CLAP PRIVATE ARM_NEON=1)
    endif()
endif()

# Add CLAP plugin demo
add_executable(clap_demo
    ../examples/clap_demo.cpp
)

target_include_directories(clap_demo PRIVATE
    ../src
    ../external/clap-sdk/include
)

target_compile_features(clap_demo PRIVATE cxx_std_17)
target_link_libraries(clap_demo Threads::Threads clap_headers)

if(MSVC)
    target_compile_options(clap_demo PRIVATE /W4)
else()
    target_compile_options(clap_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "KhDetector CLAP build configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Decimation factor: ${DECIM_FACTOR}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "")
message(STATUS "To build CLAP plugin:")
message(STATUS "  cmake --build . --target KhDetector_CLAP")
message(STATUS "")
message(STATUS "To build and run CLAP demo:")
message(STATUS "  cmake --build . --target clap_demo")
message(STATUS "  ./clap_demo") 