# Texture Generator Tool
cmake_minimum_required(VERSION 3.20)

project(GenerateTextures)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find vcpkg packages
find_package(Stb REQUIRED)

# Create executable
add_executable(GenerateTextures GenerateTextures.cpp)

# Link libraries
target_include_directories(GenerateTextures PRIVATE ${Stb_INCLUDE_DIR})

# Set output directory to MyEngine root for easy execution
set_target_properties(GenerateTextures PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/.."
)

message(STATUS "Texture Generator tool configured")
