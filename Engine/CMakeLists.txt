# Version
set (CLEAR_ENGINE_VERSION_MAJOR 0)
set (CLEAR_ENGINE_VERSION_MINOR 0)
set (CLEAR_ENGINE_VERSION_PATCH 0) 
set (CLEAR_ENGINE_CURRENT_RELEASE_YEAR 2026)

option(TYR_BUILD_TESTS "If true, build targets for running unit tests will be included in the output." OFF)

option(TYR_EDITOR_MODE "If true, the editor specific code will be generated and compiled." ON)

option(TYR_FINAL_MODE "If true, the app will be configured for the final / shippable version." OFF)

# Enforce mutual exclusion
if(TYR_EDITOR_MODE AND TYR_FINAL_MODE)
    message(FATAL_ERROR "TYR_EDITOR_MODE and TYR_FINAL_MODE cannot both be ON at the same time")
endif()

## Generate config files
configure_file("${TYR_ENGINE_DIR}/CMake/Config/BuildConfig.h.in" "${PROJECT_BINARY_DIR}/Generated/Config/BuildConfig.h")

# ThirdParty depends on TYR_EDITOR_MODE (above) to decide what to build, and needs to
# run before Runtime so editor-only targets like fastgltf exist by the time TyrantEngine
# tries to link against them.
add_subdirectory(${TYR_ROOT_DIR}/ThirdParty ${CMAKE_BINARY_DIR}/ThirdParty)

add_subdirectory(Tools)
add_subdirectory(Runtime)

## Tests
if(TYR_BUILD_TESTS)
	enable_testing()
	## Add  more here later
endif()
