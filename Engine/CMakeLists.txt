# Version
set (CLEAR_ENGINE_VERSION_MAJOR 0)
set (CLEAR_ENGINE_VERSION_MINOR 0)
set (CLEAR_ENGINE_VERSION_PATCH 0) 
set (CLEAR_ENGINE_CURRENT_RELEASE_YEAR 2023)

option(TYR_BUILD_TESTS "If true, build targets for running unit tests will be included in the output." OFF)

option(TYR_EDITOR_MODE "If true, the editor will be opened at runtime." ON)

option(TYR_FINAL_MODE "If true, the app will be configured for the final / shippable version." OFF)

# Enforce mutual exclusion
if(TYR_EDITOR_MODE AND TYR_FINAL_MODE)
    message(FATAL_ERROR "TYR_EDITOR_MODE and TYR_FINAL_MODE cannot both be ON at the same time")
endif()

## Generate config files
configure_file("${TYR_ENGINE_DIR}/CMake/Config/BuildConfig.h.in" "${PROJECT_BINARY_DIR}/Generated/Config/BuildConfig.h")

add_subdirectory(Tools)
add_subdirectory(Runtime)

## Tests
if(TYR_BUILD_TESTS)
	enable_testing()
	## Add  more here later
endif()
