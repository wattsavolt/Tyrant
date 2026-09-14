## simdjson, fastgltf and zstd only exist to support model import (glTF/GLB parsing and
## mesh chunk compression), which is editor-only (same reasoning as the Importing folder
## and NVTT in Engine/Runtime/Engine's CMakeLists.txt) - none of them are built at all
## when TYR_EDITOR_MODE is off.
if (TYR_EDITOR_MODE)
	## simdjson - vendored as the official amalgamated single-header/source distribution
	## (see ThirdParty/Source/simdjson). Version pinned to v3.12.3 to match what fastgltf
	## itself targets, so fastgltf just links this target instead of downloading its own copy.
	add_library(simdjson STATIC ${TYR_THIRD_PARTY_SOURCE_DIR}/simdjson/simdjson.cpp)
	add_library(simdjson::simdjson ALIAS simdjson)

	target_include_directories(simdjson PUBLIC ${TYR_THIRD_PARTY_SOURCE_DIR}/simdjson)
	target_compile_features(simdjson PUBLIC cxx_std_17)

	if(MSVC)
		# Vendored code - don't hold it to our own warning level.
		target_compile_options(simdjson PRIVATE /w)
		# Our own targets (see add_common_properties in HelperFunctions.cmake) always
		# build with checked iterators off, in every config including Debug. simdjson
		# doesn't get that automatically, so without this its object files disagree with
		# the rest of the program about _ITERATOR_DEBUG_LEVEL and the linker refuses to
		# combine them.
		target_compile_definitions(simdjson PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()

	set_target_properties(simdjson PROPERTIES FOLDER ThirdParty)

	## fastgltf - submodule, pinned to v0.9.0 to match the simdjson version above.
	## Its own CMakeLists links simdjson::simdjson automatically since that target
	## already exists by the time its add_subdirectory runs.
	set(FASTGLTF_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
	set(FASTGLTF_ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
	set(FASTGLTF_ENABLE_DOCS OFF CACHE BOOL "" FORCE)

	## fastgltf's own CMakeLists always registers install(EXPORT ...) rules for
	## itself, which fail CMake's export validation because it privately links our
	## simdjson target and simdjson isn't part of any install/export set. We never
	## install fastgltf as its own redistributable package - it's only ever
	## statically linked straight into our own build - so skip its install rules.
	set(CMAKE_SKIP_INSTALL_RULES ON)
	add_subdirectory(fastgltf)
	set(CMAKE_SKIP_INSTALL_RULES OFF)

	if(MSVC)
		# Same reasoning as simdjson above - keep this target's _ITERATOR_DEBUG_LEVEL in
		# sync with the rest of the program so the linker doesn't reject the mix.
		target_compile_definitions(fastgltf PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()

	set_target_properties(fastgltf PROPERTIES FOLDER ThirdParty)

	## zstd - submodule, pinned to v1.5.7. Static lib only, and we don't need the CLI
	## tool/tests/contrib code that ships alongside the library itself.
	set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)

	## Same reasoning as fastgltf above - we only ever statically link zstd straight
	## into our own build, never install it as its own redistributable package.
	set(CMAKE_SKIP_INSTALL_RULES ON)
	add_subdirectory(zstd/build/cmake)
	set(CMAKE_SKIP_INSTALL_RULES OFF)

	if(MSVC)
		# Vendored code - don't hold it to our own warning level.
		target_compile_options(libzstd_static PRIVATE /w)
		# Same reasoning as simdjson/fastgltf above - keep this target's
		# _ITERATOR_DEBUG_LEVEL in sync so the linker doesn't reject the mix.
		target_compile_definitions(libzstd_static PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()

	set_target_properties(libzstd_static PROPERTIES FOLDER ThirdParty)
	set_target_properties(libzstd PROPERTIES FOLDER ThirdParty)
endif()
