cmake_minimum_required(VERSION 3.20)
project(MyEngine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── vcpkg packages ─────────────────────────────────────────
find_package(SDL2   CONFIG REQUIRED)
find_package(glad   CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

# ── Source files ───────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/rendering/Window.cpp
    src/rendering/Shader.cpp
    src/rendering/ModelLoader.cpp
    src/core/GameTime.cpp
    src/core/Input.cpp
)

add_executable(MyEngine ${SOURCES})

# ── Include root ───────────────────────────────────────────
target_include_directories(MyEngine PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# ── Link libraries ─────────────────────────────────────────
target_link_libraries(MyEngine PRIVATE
    SDL2::SDL2
    SDL2::SDL2main
    glad::glad
    glm::glm
    assimp::assimp
)

# ── Copy shaders to build output ───────────────────────────
add_custom_command(TARGET MyEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:MyEngine>/shaders
    COMMENT "Copying shaders to build output..."
)