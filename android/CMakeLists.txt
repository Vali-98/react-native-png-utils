project(PngUtils)
cmake_minimum_required(VERSION 3.9.0)

set (PACKAGE_NAME PngUtils)
set (CMAKE_VERBOSE_MAKEFILE ON)
set (CMAKE_CXX_STANDARD 20)

# Enable Raw Props parsing in react-native (for Nitro Views)
add_compile_options(-DRN_SERIALIZABLE_STATE=1 -O3)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED 
    src/main/cpp/cpp-adapter.cpp
    ../cpp/turbob64/turbob64c.c
    ../cpp/turbob64/turbob64d.c
    ../cpp/HybridPngUtils.hpp
    ../cpp/HybridPngUtils.cpp
    ../cpp/HybridAudioUtils.cpp
    ../cpp/audio/WavWriter.cpp
    ../cpp/audio/AudioDecoder.android.cpp
)

# Validate every base64 block, including metadata that may be plain text.
target_compile_definitions(${PACKAGE_NAME} PRIVATE B64CHECK UA_MEMCPY)
# Turbo-Base64 is an implementation detail. Hidden C symbols let the linker
# discard unused scalar entry points/tables and bind retained calls locally.
set_target_properties(${PACKAGE_NAME} PROPERTIES C_VISIBILITY_PRESET hidden)

# Conditional NEON setup (only for arm64-v8a)
if(ANDROID_ABI STREQUAL "arm64-v8a")
    target_compile_definitions(${PACKAGE_NAME} PRIVATE PNGUTIL_ENABLE_NEON)
    target_sources(${PACKAGE_NAME} PRIVATE ../cpp/turbob64/turbob64v128.c)
endif()

# Add Nitrogen specs :)
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/PngUtils+autolinking.cmake)


# Set up local includes
include_directories(
        "src/main/cpp"
        "../cpp"
)

find_library(LOG_LIB log)
find_library(Z_LIB z)
target_link_libraries(  
        ${PACKAGE_NAME}
        ${LOG_LIB}
        ${Z_LIB}
        mediandk
        android                                   # <-- Android core
)
