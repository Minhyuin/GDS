cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

include("tools/cmake/adapter.cmake")
include("tools/cmake/FindInclude.cmake")
include("tools/cmake/ccache.cmake")
include("tools/cmake/Version.cmake")
include("tools/cmake/ShowMessage.cmake")

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME GDS)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C/CPP languages
enable_language(C CXX ASM)

set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/user/lib")

add_compile_options($<$<C_COMPILER_ID:GNU>:-fdiagnostics-color=always>)
add_compile_options($<$<CXX_COMPILER_ID:GNU>:-fdiagnostics-color=always>)
add_compile_options(-Wno-unused-function)
add_compile_options(-Wno-pedantic)

# Create an executable object type
file(GLOB_RECURSE USER_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/user/*.c"
    "${CMAKE_SOURCE_DIR}/user/*.cc"
    "${CMAKE_SOURCE_DIR}/user/*.cpp"
    "${CMAKE_SOURCE_DIR}/user/*.S"
)
list(FILTER USER_SOURCES EXCLUDE REGEX ".*/user/lib(/.*)?$")
add_executable(${CMAKE_PROJECT_NAME} ${USER_SOURCES})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add SEGGER RTT/SystemView sources
include("tools/cmake/Segger.cmake")

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
)

# Add include paths
FIND_INCLUDE_DIR(USER_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/user")
list(FILTER USER_INCLUDE_DIRS EXCLUDE REGEX ".*/user/lib(/.*)?$")
list(REMOVE_DUPLICATES USER_INCLUDE_DIRS)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    ${USER_INCLUDE_DIRS}
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    segger

    # Add user defined libraries
)

showMessage()
