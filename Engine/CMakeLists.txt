# Version
set (CLEAR_ENGINE_VERSION_MAJOR 0)
set (CLEAR_ENGINE_VERSION_MINOR 0)
set (CLEAR_ENGINE_VERSION_PATCH 0) 
set (CLEAR_ENGINE_CURRENT_RELEASE_YEAR 2023)

option(TYR_BUILD_TESTS "If true, build targets for running unit tests will be included in the output." OFF)

option(TYR_EDITOR_MODE "If true, the editor will be opened at runtime." ON)

cmake_dependent_option(TYR_FINAL_MODE "If true, the app will be configured for the final version." OFF "NOT TYR_EDITOR_MODE" OFF)

## Generate config files
configure_file("${TYR_ENGINE_DIR}/CMake/Config/EngineConfig.h.in" "${PROJECT_BINARY_DIR}/Generated/Engine/EngineConfig.h")

add_subdirectory(Tools)
add_subdirectory(Runtime)

## Tests
if(TYR_BUILD_TESTS)
	enable_testing()
	## Add  more here later
endif()
