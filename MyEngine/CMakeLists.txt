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
if(NOT DEFINED assimp_DIR)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/assimp")
        set(assimp_DIR "C:/vcpkg/installed/x64-windows/share/assimp")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/assimp")
        set(assimp_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/assimp")
    endif()
endif()

# Try find_package first (works with config-mode packages)
find_package(Assimp CONFIG QUIET)

# ============================================================
# Source Files
# ============================================================
set(SOURCES
    src/main.cpp

    src/rendering/Shader.cpp
    src/rendering/Mesh.cpp
    src/rendering/Model.cpp
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


# If Assimp target was found, link it. Otherwise try fallback to vcpkg installed lib.
if(TARGET assimp::assimp)
    target_link_libraries(MyEngine PRIVATE assimp::assimp)
else()
    if(EXISTS "C:/vcpkg/installed/x64-windows/lib/assimp-vc145-mt.lib")
        target_include_directories(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/include")
        target_link_libraries(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/lib/assimp-vc145-mt.lib")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/assimp-vc145-mt.lib")
        target_include_directories(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
        target_link_libraries(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/assimp-vc145-mt.lib")
    else()
        message(WARNING "Assimp not found as CMake target and no fallback library found. Model loading will fail at link time.")
    endif()
endif()

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

# Copy vcpkg runtime DLLs next to the executable so the app can find them at runtime
if(EXISTS "C:/vcpkg/installed/x64-windows/bin")
    add_custom_command(TARGET MyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying vcpkg runtime DLLs to output folder..."
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "C:/vcpkg/installed/x64-windows/bin"
            $<TARGET_FILE_DIR:MyEngine>
        COMMENT "Copying vcpkg runtime DLLs..."
    )
endif()
