cmake_minimum_required (VERSION 3.25.1)

set(TYR_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/DefaultApp CACHE PATH "The path to the app's directory.")
# The app is named after its folder.
string(REGEX REPLACE "[/\\]+$" "" TYR_APP_DIR_TRIMMED "${TYR_APP_DIR}")
cmake_path(GET TYR_APP_DIR_TRIMMED FILENAME TYR_APP_NAME)
set(TYR_APP_LIB_NAME ${TYR_APP_NAME}_Lib)
unset(TYR_APP_NAME CACHE)

project (${TYR_APP_NAME})

set (TYR_ROOT_DIR ${PROJECT_SOURCE_DIR})
set (TYR_THIRD_PARTY_DIR ${TYR_ROOT_DIR}/ThirdParty)
set (TYR_THIRD_PARTY_COMPILED_LIBS_DIR ${TYR_THIRD_PARTY_DIR}/Compiled)
set (TYR_THIRD_PARTY_HEADER_LIBS_DIR ${TYR_THIRD_PARTY_DIR}/Header)
set (TYR_THIRD_PARTY_SOURCE_DIR ${TYR_THIRD_PARTY_DIR}/Source)
set (TYR_ENGINE_DIR ${TYR_ROOT_DIR}/Engine)
set (TYR_RUNTIME_DIR ${TYR_ENGINE_DIR}/Runtime)
set (TYR_TOOLS_DIR ${TYR_ENGINE_DIR}/Tools)
# When the app is renamed, an install folder still named after the old app follows the new name.
if(DEFINED TYR_PREVIOUS_APP_NAME AND NOT TYR_PREVIOUS_APP_NAME STREQUAL TYR_APP_NAME)
	if(CMAKE_INSTALL_PREFIX MATCHES "/${TYR_PREVIOUS_APP_NAME}$")
		string(REGEX REPLACE "/${TYR_PREVIOUS_APP_NAME}$" "/${TYR_APP_NAME}" TYR_NEW_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
		set(CMAKE_INSTALL_PREFIX ${TYR_NEW_INSTALL_PREFIX} CACHE PATH "Install path prefix, prepended onto install directories." FORCE)
	endif()
endif()
set(TYR_PREVIOUS_APP_NAME ${TYR_APP_NAME} CACHE INTERNAL "The app name from the last configure.")
set (TYR_APP_ASSETS_DIR ${TYR_APP_DIR}/Assets)
set (TYR_CONFIG_DIR ${TYR_APP_DIR}/Config)
message(STATUS "App: ${TYR_APP_NAME} (${TYR_APP_LIB_NAME}), assets: ${TYR_APP_ASSETS_DIR}, config: ${TYR_CONFIG_DIR}")
# The engine's own content before it's imported. Editor only.
set (TYR_SOURCE_ASSETS_DIR ${TYR_ROOT_DIR}/SourceAssets CACHE STRING "The path to the engine's source (pre-import) asset directory.")
set (TYR_FINAL_BIN_DIR "")

# Default install directory
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(LINUX)
		set(CMAKE_INSTALL_PREFIX /usr/local/opt/TyrantEngine CACHE PATH "..." FORCE)
	endif()
endif()

# Determine architecture
set(TYR_ARCHITECTURE "unknown")
## Normalize processor string
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ARCH)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    if(ARCH MATCHES "^(x86_64|amd64)$")
        set(TYR_ARCHITECTURE "x64")
    elseif(ARCH MATCHES "^(aarch64|arm64)$")
        set(TYR_ARCHITECTURE "ARM64")
    else()
        set(TYR_ARCHITECTURE "unknown_64bit")
    endif()
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    if(ARCH MATCHES "^(x86|i[3-6]86)$")
        set(TYR_ARCHITECTURE "Win32")
    elseif(ARCH MATCHES "^arm$")
        set(TYR_ARCHITECTURE "ARM32")
    else()
        set(TYR_ARCHITECTURE "unknown_32bit")
    endif()
endif()

include(CMakeDependentOption)
include(${TYR_ENGINE_DIR}/CMake/Config/Properties.cmake)
include(${TYR_ENGINE_DIR}/CMake/Scripts/HelperFunctions.cmake)

# The app can live outside the engine folder, so it needs its own build folder.
add_subdirectory(${TYR_APP_DIR} ${CMAKE_BINARY_DIR}/${TYR_APP_NAME})
add_subdirectory(Engine)