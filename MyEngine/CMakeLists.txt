cmake_minimum_required(VERSION 3.20)

project(MyEngine LANGUAGES CXX)

# ============================================================
# C++ Settings
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# Required Packages
# ============================================================
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# ============================================================
# Source Files
# ============================================================
set(SOURCES
    src/main.cpp

    src/rendering/Shader.cpp
    src/rendering/Mesh.cpp
    src/rendering/MeshPrimitives.cpp
    src/core/Input.cpp
    
    src/ecs/Scene.cpp

    src/systems/MeshRendererSystem.cpp
    src/systems/CameraSystem.cpp
)

# ============================================================
# Create Executable
# ============================================================
add_executable(MyEngine ${SOURCES})

# ============================================================
# Include Directories
# ============================================================
target_include_directories(MyEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# Link Libraries
# ============================================================
target_link_libraries(MyEngine PRIVATE
    OpenGL::GL
    glad::glad
    glfw
    glm::glm
)

# ============================================================
# Compile Definitions
# ============================================================
target_compile_definitions(MyEngine PRIVATE
    GLM_ENABLE_EXPERIMENTAL
)

# ============================================================
# Copy shaders folder next to executable
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
    add_custom_command(TARGET MyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:MyEngine>/shaders
        COMMENT "Copying shaders to build output..."
    )
endif()