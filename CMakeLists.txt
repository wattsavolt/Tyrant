cmake_minimum_required (VERSION 3.25.1)

set(TYR_APP_NAME DefaultApp CACHE STRING "The name of the app.") 
set(TYR_APP_LIB_NAME ${TYR_APP_NAME}_Lib CACHE STRING "The name of the app's dll.")

project (${TYR_APP_NAME})

set (TYR_ROOT_DIR ${PROJECT_SOURCE_DIR})
set (TYR_THIRD_PARTY_DIR ${TYR_ROOT_DIR}/ThirdParty)
set (TYR_THIRD_PARTY_EXPLICIT_DIR ${TYR_THIRD_PARTY_DIR}/Explicit)
set (TYR_THIRD_PARTY_IMPLICIT_DIR ${TYR_THIRD_PARTY_DIR}/Implicit)
set (TYR_THIRD_PARTY_INCLUDE_DIR ${TYR_THIRD_PARTY_DIR}/Include)
set (TYR_ENGINE_DIR ${TYR_ROOT_DIR}/Engine)
set (TYR_RUNTIME_DIR ${TYR_ENGINE_DIR}/Runtime)
set (TYR_TOOLS_DIR ${TYR_ENGINE_DIR}/Tools)
set (TYR_APP_DIR ${TYR_ROOT_DIR}/DefaultApp CACHE STRING "The path to the app's directory.") 
set (TYR_APP_ASSETS_DIR ${TYR_APP_DIR}/Assets CACHE STRING "The path to the app's asset directory.")
set (TYR_FINAL_BIN_DIR "")

# Default install directory
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	if(LINUX)
		set(CMAKE_INSTALL_PREFIX /usr/local/opt/TyrantEngine CACHE PATH "..." FORCE)
	endif()
endif()

include(CMakeDependentOption)
include(${TYR_ENGINE_DIR}/CMake/Config/Properties.cmake)
include(${TYR_ENGINE_DIR}/CMake/Scripts/HelperFunctions.cmake)

add_subdirectory(${TYR_APP_DIR})
add_subdirectory(Engine)