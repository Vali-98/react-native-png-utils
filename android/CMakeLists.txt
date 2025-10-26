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
)

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
        android                                   # <-- Android core
)
