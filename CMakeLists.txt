cmake_minimum_required(VERSION 3.8)

# Project name
project("Midori")

# Enforce C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
if (MSVC)
    add_compile_options(/W4 /permissive-)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /Zi")
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

# Source files for the main executable
file(GLOB_RECURSE SRC_FILES
    "src/Common/*.h"
    "src/Compiler/*.h"
    "src/Interpreter/*.h"
    "src/Utility/*.h"
    "src/Common/*.cpp"
    "src/Compiler/*.cpp"
    "src/Interpreter/*.cpp"
    "src/Utility/*.cpp"
    "src/Midori.cpp"
)

# Add source to this project's executable.
add_executable(Midori ${SRC_FILES})

# Source files for the standard library DLL
file(GLOB_RECURSE STD_LIB_SRC_FILES
    "src/Common/Value/Value.h"
    "src/Common/Value/Value.cpp"
    "src/Library/MidoriStdLib.cpp"
)

# Add standard library as a dynamically linked library
add_library(MidoriStdLib SHARED ${STD_LIB_SRC_FILES})

# Set the output name for Unix-like systems to follow naming convention
if (UNIX)
    set_target_properties(MidoriStdLib PROPERTIES OUTPUT_NAME "libMidoriStdLib")
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

# Link against any necessary libraries
# target_link_libraries(MidoriStdLib <OtherLibraries>)