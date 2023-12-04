cmake_minimum_required(VERSION 3.8)

# Project name
project("Midori")

# Enforce C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
if (MSVC)
    add_compile_options(/W4 /permissive-)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ox /Zi")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
else()
    add_compile_options(-Wall -Wextra -pedantic -Werror -Wconversion)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -g")
endif()

# Debug flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# Source files for the main executable including MidoriStdLib
file(GLOB_RECURSE SRC_FILES
    "src/*.h"
    "src/*.cpp"
    "src/Library/*.h"
    "src/Library/*.cpp"
)

# Add source to this project's executable.
add_executable(Midori ${SRC_FILES})

# Set the output name for Unix-like systems to follow naming convention
if (UNIX)
    set_target_properties(Midori PROPERTIES OUTPUT_NAME "libMidoriStdLib")
endif()

# Specify where to put the build artifacts (this makes sure they go into 'out')
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()

# Set debug information format for MSVC
if (MSVC)
  target_compile_options(Midori PRIVATE 
    $<$<CONFIG:RelWithDebInfo>:/Zi>
    $<$<CONFIG:Release>:/Zi>
  )
  target_link_options(Midori PRIVATE
    $<$<CONFIG:RelWithDebInfo>:/DEBUG /OPT:REF /OPT:ICF>
    $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>
  )
endif()