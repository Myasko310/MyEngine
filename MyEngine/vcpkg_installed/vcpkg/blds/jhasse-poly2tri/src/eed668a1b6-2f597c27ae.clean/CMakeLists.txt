cmake_minimum_required(VERSION 3.12)

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN TRUE)

project(poly2tri LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 14)

set(INSTALL_BIN_DIR      "bin"                     CACHE PATH "Path where exe and dll will be installed")
set(INSTALL_LIB_DIR      "lib"                     CACHE PATH "Path where lib will be installed")
set(INSTALL_INCLUDE_DIR  "include/${PROJECT_NAME}" CACHE PATH "Path where headers will be installed")
set(INSTALL_CMAKE_DIR    "share/${PROJECT_NAME}"   CACHE PATH "Path where cmake configs will be installed")

option(P2T_BUILD_TESTS "Build tests" OFF)
option(P2T_BUILD_TESTBED "Build the testbed application" OFF)

file(GLOB SOURCES poly2tri/common/*.cc poly2tri/sweep/*.cc)
file(GLOB HEADERS poly2tri/*.h poly2tri/common/*.h poly2tri/sweep/*.h)

add_library(poly2tri ${SOURCES} ${HEADERS})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/poly2tri>
    $<INSTALL_INTERFACE:include>
    $<INSTALL_INTERFACE:include/${PROJECT_NAME}>
)

get_target_property(poly2tri_target_type poly2tri TYPE)
if(poly2tri_target_type STREQUAL SHARED_LIBRARY)
  target_compile_definitions(poly2tri PRIVATE P2T_SHARED_EXPORTS)
else()
  target_compile_definitions(poly2tri PUBLIC P2T_STATIC_EXPORTS)
endif()

if(P2T_BUILD_TESTS)
    enable_testing()
    add_subdirectory(unittest)
endif()

if(P2T_BUILD_TESTBED)
    add_subdirectory(testbed)
endif()

install(DIRECTORY poly2tri DESTINATION include FILES_MATCHING PATTERN "*.h")

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Targets
  RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
)

install (EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Config.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION "${INSTALL_CMAKE_DIR}"
)
