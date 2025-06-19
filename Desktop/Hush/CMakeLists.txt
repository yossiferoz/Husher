cmake_minimum_required(VERSION 3.20)
project(KhDetector VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
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

# Find OpenGL
find_package(OpenGL REQUIRED)

# Add VSTGUI as a subdirectory (without examples)
set(VSTGUI_STANDALONE OFF)
set(VSTGUI_TOOLS OFF)
set(VSTGUI_DISABLE_UNITTESTS ON)
add_subdirectory(external/vstgui)

# Add VST3 SDK submodule (minimal, no examples)
set(SMTG_ADD_VST3_HOSTING_SAMPLES OFF)
set(SMTG_ADD_VST3_PLUGINS_SAMPLES OFF)
add_subdirectory(external/vst3sdk EXCLUDE_FROM_ALL)

# VST3 Plugin target
add_library(KhDetector_VST3 MODULE)

# VST3 Plugin sources
target_sources(KhDetector_VST3 PRIVATE
    src/KhDetectorProcessor.cpp
    src/KhDetectorController.cpp
    src/KhDetectorFactory.cpp
    src/KhDetectorVersion.h
    src/RingBuffer.h
    src/PolyphaseDecimator.h
    src/RealtimeThreadPool.cpp
    src/AiInference.cpp
    src/PostProcessor.cpp
    src/MidiEventHandler.cpp
    src/WaveformData.cpp
    src/WaveformRenderer.cpp
    src/KhDetectorOpenGLView.cpp
    src/KhDetectorGUIView.cpp
    src/KhDetectorEditor.cpp
)

# VST3 Include directories
target_include_directories(KhDetector_VST3 PRIVATE
    src
    external/vst3sdk
    external/vst3sdk/pluginterfaces
    external/vst3sdk/public.sdk
    external/vst3sdk/base
    external/vstgui
    external/vstgui/vstgui
)

# VST3 Link with VST3 SDK and VSTGUI
target_link_libraries(KhDetector_VST3 PRIVATE
    sdk
    pluginterfaces
    vstgui
    vstgui_uidescription
    Threads::Threads
    OpenGL::GL
)

# VST3 Compiler-specific flags
target_compile_features(KhDetector_VST3 PRIVATE cxx_std_17)

# VST3 Platform-specific settings
if(APPLE)
    set_target_properties(KhDetector_VST3 PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "vst3"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "KhDetector"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024"
        XCODE_ATTRIBUTE_WRAPPER_EXTENSION "vst3"
        XCODE_ATTRIBUTE_GENERATE_PKGINFO_FILE "YES"
    )
    find_library(COCOA_LIBRARY Cocoa)
    find_library(OPENGL_LIBRARY OpenGL)
    target_link_libraries(KhDetector_VST3 PRIVATE ${COCOA_LIBRARY} ${OPENGL_LIBRARY})
elseif(WIN32)
    set_target_properties(KhDetector_VST3 PROPERTIES
        SUFFIX ".vst3"
    )
    target_link_libraries(KhDetector_VST3 PRIVATE opengl32 gdi32 user32)
else()
    set_target_properties(KhDetector_VST3 PROPERTIES
        SUFFIX ".vst3"
    )
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    target_link_libraries(KhDetector_VST3 PRIVATE ${GTK3_LIBRARIES} GL)
    target_include_directories(KhDetector_VST3 PRIVATE ${GTK3_INCLUDE_DIRS})
endif()

# VST3 Compiler settings
if(MSVC)
    target_compile_options(KhDetector_VST3 PRIVATE /W4)
    target_compile_definitions(KhDetector_VST3 PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        DEVELOPMENT=1
    )
    # Enable AVX/SSE on Windows
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
        target_compile_options(KhDetector_VST3 PRIVATE /arch:AVX)
    endif()
else()
    target_compile_options(KhDetector_VST3 PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_definitions(KhDetector_VST3 PRIVATE DEVELOPMENT=1)
    # Enable SIMD optimizations
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(KhDetector_VST3 PRIVATE -msse2 -msse4.1 -mavx)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        target_compile_options(KhDetector_VST3 PRIVATE -march=armv8-a)
    endif()
endif()

message(STATUS "KhDetector VST3 build configured")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Decimation factor: ${DECIM_FACTOR}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}") 