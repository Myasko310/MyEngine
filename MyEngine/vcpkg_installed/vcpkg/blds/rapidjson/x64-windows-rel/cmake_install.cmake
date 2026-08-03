# Install script for directory: C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/src/106cf7102f-b0877c68bd.clean

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "OFF")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "doc" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/share/doc/RapidJSON/readme.md")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/share/doc/RapidJSON" TYPE FILE FILES "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/src/106cf7102f-b0877c68bd.clean/readme.md")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/include/rapidjson")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/include" TYPE DIRECTORY FILES "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/src/106cf7102f-b0877c68bd.clean/include/rapidjson")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "examples" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/share/doc/RapidJSON/examples/")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/share/doc/RapidJSON/examples" TYPE DIRECTORY FILES "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/src/106cf7102f-b0877c68bd.clean/example/" REGEX "/cmakefiles$" EXCLUDE REGEX "/makefile$" EXCLUDE REGEX "/cmake\\_install\\.cmake$" EXCLUDE)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/lib/cmake/RapidJSON/RapidJSONConfig.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/lib/cmake/RapidJSON" TYPE FILE FILES "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/CMakeFiles/RapidJSONConfig.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSONConfig.cmake;C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSONConfigVersion.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake" TYPE FILE FILES
    "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/RapidJSONConfig.cmake"
    "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/RapidJSONConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSON-targets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSON-targets.cmake"
         "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/CMakeFiles/Export/ebc6702728418e6c3cad470fb2237378/RapidJSON-targets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSON-targets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSON-targets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake/RapidJSON-targets.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/pkgs/rapidjson_x64-windows/cmake" TYPE FILE FILES "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/CMakeFiles/Export/ebc6702728418e6c3cad470fb2237378/RapidJSON-targets.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/micha/source/repos/MyEngine/MyEngine/vcpkg_installed/vcpkg/blds/rapidjson/x64-windows-rel/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
