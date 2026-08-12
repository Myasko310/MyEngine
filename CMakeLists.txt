cmake_minimum_required(VERSION 3.20)

project(MyEngine LANGUAGES CXX)

# If a vcpkg toolchain is available on this machine, prefer it so
# Visual Studio / CMake will use vcpkg-installed packages by default.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        message(STATUS "Auto-detected vcpkg toolchain at C:/vcpkg/scripts/buildsystems/vcpkg.cmake; enabling vcpkg toolchain.")
        set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file" FORCE)
    endif()
endif()

# Ensure CMake can find vcpkg-provided config files under the installed share
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};C:/vcpkg/installed/x64-windows/share")
else()
    list(APPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows/share")
endif()

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
find_package(glad CONFIG QUIET)

# Fallback: prefer vcpkg-installed glad if available
if(NOT DEFINED glad_DIR)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/glad")
        set(glad_DIR "C:/vcpkg/installed/x64-windows/share/glad")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glad")
        set(glad_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glad")
    endif()
endif()

# Try find_package again now that we may have set glad_DIR
find_package(glad CONFIG QUIET)
find_package(glfw3 CONFIG QUIET)

# Fallback: prefer vcpkg-installed glfw3 if available
if(NOT DEFINED glfw3_DIR)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/glfw3")
        set(glfw3_DIR "C:/vcpkg/installed/x64-windows/share/glfw3")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glfw3")
        set(glfw3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glfw3")
    endif()
endif()

# Try find_package again for glfw3
find_package(glfw3 CONFIG QUIET)
find_package(glm CONFIG QUIET)

# Fallback: prefer vcpkg-installed glm if available
if(NOT DEFINED glm_DIR)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/glm")
        set(glm_DIR "C:/vcpkg/installed/x64-windows/share/glm")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glm")
        set(glm_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/glm")
    endif()
endif()

# Try find_package again for glm
find_package(glm CONFIG QUIET)
find_package(imgui CONFIG QUIET)
if(NOT DEFINED assimp_DIR)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/assimp")
        set(assimp_DIR "C:/vcpkg/installed/x64-windows/share/assimp")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/assimp")
        set(assimp_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/assimp")
    endif()
endif()

# Try find_package first (works with config-mode packages)
find_package(Assimp CONFIG QUIET)
find_package(imgui CONFIG QUIET)

# Stb ships a FindStb.cmake module (not a Config package), so it must be
# discoverable via CMAKE_MODULE_PATH rather than CMAKE_PREFIX_PATH. The module
# also needs Stb_DIR pointed at the vcpkg install prefix so it can locate
# stb_image.h under <prefix>/include.
if(EXISTS "C:/vcpkg/installed/x64-windows/share/stb")
    list(APPEND CMAKE_MODULE_PATH "C:/vcpkg/installed/x64-windows/share/stb")
    set(Stb_DIR "C:/vcpkg/installed/x64-windows")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/stb")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/stb")
    set(Stb_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows")
endif()
find_package(Stb REQUIRED)

find_package(imguizmo CONFIG QUIET)

# OpenAL Soft (audio backend)
find_package(OpenAL CONFIG QUIET)
if(NOT TARGET OpenAL::OpenAL)
    if(EXISTS "C:/vcpkg/installed/x64-windows/share/openal-soft")
        set(OpenAL_DIR "C:/vcpkg/installed/x64-windows/share/openal-soft")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/openal-soft")
        set(OpenAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/share/openal-soft")
    endif()
    find_package(OpenAL CONFIG QUIET)
endif()

# ============================================================
# Source Files
# ============================================================
set(SOURCES
    src/main.cpp

    src/rendering/Shader.cpp
    src/rendering/Mesh.cpp
    src/rendering/Model.cpp
    src/rendering/ShadowMap.cpp
    src/rendering/PostProcessPipeline.cpp
    src/rendering/MeshPrimitives.cpp
    src/rendering/Texture.cpp
    src/renderer/FrustumCuller.cpp
    src/core/Input.cpp
    src/core/AssetManager.cpp
    src/core/Raycast.cpp
    src/core/FileDialog.cpp

    src/audio/AudioEngine.cpp
    src/audio/AudioClip.cpp

    src/ecs/Scene.cpp

    src/systems/MeshRendererSystem.cpp
    src/systems/CameraSystem.cpp
    src/systems/PhysicsSystem.cpp
    src/systems/AudioSystem.cpp
    src/systems/AnimationSystem.cpp
    src/serialization/SceneSerializer.cpp
)

# Add bundled ImGui backend implementations (glfw + opengl3) so builds
# using vcpkg's imgui (which may not install backends) can compile the
# platform/renderer bindings we need. These files live under src/vendor/imgui/backends
list(APPEND SOURCES
    src/vendor/imgui/backends/imgui_impl_glfw.cpp
    src/vendor/imgui/backends/imgui_impl_opengl3.cpp
)

# Only compile bundled backends when ImGui support is enabled (guard compile)
set_source_files_properties(
    src/vendor/imgui/backends/imgui_impl_glfw.cpp
    src/vendor/imgui/backends/imgui_impl_opengl3.cpp
    PROPERTIES
    COMPILE_DEFINITIONS "$<$<BOOL:${USE_IMGUI}>:USE_IMGUI>"
)

# ============================================================
# Create Executable
# ============================================================
add_executable(MyEngine ${SOURCES})

# Ensure the built executable outputs to the build directory per-config so
# Visual Studio/debugger runs the same binary that CMake produces and so
# our post-build DLL copy lands next to the exe.
set_target_properties(MyEngine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/Debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/Release
)
# Copy vcpkg runtime DLLs (e.g., assimp DLL) into the executable output so
# the program can run from the build folder. This helps when vcpkg provides
# dynamic libraries under installed/<triplet>/bin. We prefer the global
# C:/vcpkg location but also support a project-local vcpkg_installed folder.
if(EXISTS "C:/vcpkg/installed/x64-windows/bin")
    file(GLOB VCPKG_RUNTIME_DLLS "C:/vcpkg/installed/x64-windows/bin/*.dll")
    foreach(_dll ${VCPKG_RUNTIME_DLLS})
        add_custom_command(TARGET MyEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" $<TARGET_FILE_DIR:MyEngine>
        )
    endforeach()
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/bin")
    file(GLOB VCPKG_RUNTIME_DLLS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/*.dll")
    foreach(_dll ${VCPKG_RUNTIME_DLLS})
        add_custom_command(TARGET MyEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" $<TARGET_FILE_DIR:MyEngine>
        )
    endforeach()
endif()

# Also copy DLLs to the legacy VS CMake configure output some IDEs use so
# launching the project directly from Visual Studio finds runtime DLLs.
set(LEGACY_VS_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/build/debug/Debug")
if(EXISTS "C:/vcpkg/installed/x64-windows/bin")
    file(GLOB VCPKG_RUNTIME_DLLS_LEGACY "C:/vcpkg/installed/x64-windows/bin/*.dll")
    foreach(_dll ${VCPKG_RUNTIME_DLLS_LEGACY})
        add_custom_command(TARGET MyEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LEGACY_VS_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${LEGACY_VS_OUTPUT}"
        )
    endforeach()
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/bin")
    file(GLOB VCPKG_RUNTIME_DLLS_LEGACY "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/bin/*.dll")
    foreach(_dll ${VCPKG_RUNTIME_DLLS_LEGACY})
        add_custom_command(TARGET MyEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${LEGACY_VS_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_dll}" "${LEGACY_VS_OUTPUT}"
        )
    endforeach()
endif()

# Create a debug-suffixed copy of assimp DLL if the code expects the "-mtd" variant
if(EXISTS "C:/vcpkg/installed/x64-windows/bin/assimp-vc145-mt.dll")
    add_custom_command(TARGET MyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/vcpkg/installed/x64-windows/bin/assimp-vc145-mt.dll" $<TARGET_FILE_DIR:MyEngine>/assimp-vc145-mtd.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "C:/vcpkg/installed/x64-windows/bin/assimp-vc145-mt.dll" "${LEGACY_VS_OUTPUT}/assimp-vc145-mtd.dll"
    )
endif()

# ============================================================
# Include Directories
# ============================================================
target_include_directories(MyEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Ensure the bundled imgui backend headers are available on the include path
target_include_directories(MyEngine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/imgui/backends
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

if(TARGET OpenAL::OpenAL)
    target_link_libraries(MyEngine PRIVATE OpenAL::OpenAL)
elseif(EXISTS "C:/vcpkg/installed/x64-windows/lib/OpenAL32.lib")
    target_include_directories(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/include")
    target_link_libraries(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/lib/OpenAL32.lib")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/OpenAL32.lib")
    target_include_directories(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
    target_link_libraries(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/OpenAL32.lib")
else()
    message(WARNING "OpenAL not found as CMake target and no fallback library found. Audio will fail to link.")
endif()

if(WIN32)
    target_link_libraries(MyEngine PRIVATE comdlg32)
endif()

if(TARGET imguizmo::imguizmo)
    target_link_libraries(MyEngine PRIVATE imguizmo::imguizmo)
    target_compile_definitions(MyEngine PRIVATE USE_IMGUIZMO)
elseif(EXISTS "C:/vcpkg/installed/x64-windows/lib/imguizmo.lib")
    target_include_directories(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/include")
    target_link_libraries(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/lib/imguizmo.lib")
    target_compile_definitions(MyEngine PRIVATE USE_IMGUIZMO)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/imguizmo.lib")
    target_include_directories(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
    target_link_libraries(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/imguizmo.lib")
    target_compile_definitions(MyEngine PRIVATE USE_IMGUIZMO)
else()
    message(WARNING "ImGuizmo target not found; viewport gizmos will not be available unless imguizmo is installed via vcpkg.")
endif()

if(TARGET imgui::imgui)
    target_link_libraries(MyEngine PRIVATE imgui::imgui)
    target_compile_definitions(MyEngine PRIVATE USE_IMGUI)
else()
    # Fallback: if ImGui was installed via vcpkg but find_package didn't create an imported target,
    # detect the common vcpkg install location and link directly to the static lib.
    if(EXISTS "C:/vcpkg/installed/x64-windows/lib/imgui.lib")
        message(STATUS "ImGui: found vcpkg static lib fallback; enabling ImGui support")
        target_include_directories(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/include")
        target_link_libraries(MyEngine PRIVATE "C:/vcpkg/installed/x64-windows/lib/imgui.lib")
        target_compile_definitions(MyEngine PRIVATE USE_IMGUI)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/imgui.lib")
        message(STATUS "ImGui: found project-local vcpkg static lib fallback; enabling ImGui support")
        target_include_directories(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/include")
        target_link_libraries(MyEngine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed/x64-windows/lib/imgui.lib")
        target_compile_definitions(MyEngine PRIVATE USE_IMGUI)
    else()
        message(WARNING "ImGui target not found; UI will not be available unless ImGui is installed via vcpkg or CMake packages.")
    endif()
endif()


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

# ============================================================
# Copy assets folder next to executable
# ============================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    add_custom_command(TARGET MyEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:MyEngine>/assets
        COMMENT "Copying assets to build output..."
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

# ============================================================
# Tools subdirectory
# ============================================================
add_subdirectory(tools)
