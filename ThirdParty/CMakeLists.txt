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
	add_subdirectory(Submodule/fastgltf)
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
	add_subdirectory(Submodule/zstd/build/cmake)
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

	## meshoptimizer - submodule, pinned to v1.2. Used for mesh geometry processing at
	## import time (meshlet generation). Static lib only; we don't need its demo/gltfpack
	## tools or its own install/export rules any more than fastgltf/zstd do above.
	set(MESHOPT_BUILD_DEMO OFF CACHE BOOL "" FORCE)
	set(MESHOPT_BUILD_GLTFPACK OFF CACHE BOOL "" FORCE)
	set(MESHOPT_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	set(MESHOPT_WERROR OFF CACHE BOOL "" FORCE)
	set(MESHOPT_INSTALL OFF CACHE BOOL "" FORCE)

	set(CMAKE_SKIP_INSTALL_RULES ON)
	add_subdirectory(Submodule/meshoptimizer)
	set(CMAKE_SKIP_INSTALL_RULES OFF)

	if(MSVC)
		# Vendored code - don't hold it to our own warning level.
		target_compile_options(meshoptimizer PRIVATE /w)
		# Same reasoning as simdjson/fastgltf/zstd above - keep this target's
		# _ITERATOR_DEBUG_LEVEL in sync so the linker doesn't reject the mix.
		target_compile_definitions(meshoptimizer PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()

	set_target_properties(meshoptimizer PROPERTIES FOLDER ThirdParty)
endif()

## Jolt Physics - submodule, pinned to v5.6.0. Runtime physics, so always built. Only the core
## library is built (its samples/tests only build when it's the root project), with our own
## compiler flags, the DLL runtime, and no GPU compute, debug renderer or profiler.
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "" FORCE)
set(OVERRIDE_CXX_FLAGS OFF CACHE BOOL "" FORCE)
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED OFF CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(Submodule/JoltPhysics/Build)

if(MSVC)
	# Vendored code - don't hold it to our own warning level.
	target_compile_options(Jolt PRIVATE /w)
	# Same reasoning as the libraries above - keep _ITERATOR_DEBUG_LEVEL in sync for the linker.
	target_compile_definitions(Jolt PRIVATE _ITERATOR_DEBUG_LEVEL=0)
endif()

set_target_properties(Jolt PROPERTIES FOLDER ThirdParty)

## Dear ImGui - submodule, pinned to v1.92.9. Editor-tool UI only, driven through the engine's
## own GUIModule - so only built when TYR_FINAL_MODE is off, same as GUIModule.cpp's own
## #if !TYR_FINAL gate on the C++ side. No official Vulkan/Win32 backend files are used here -
## GUIModule renders through Tyrant's own RenderAPI instead (see Renderer/Rendering/GUIPass),
## so only the core library itself needs building.
if (NOT TYR_FINAL_MODE)
	add_library(imgui STATIC
		Submodule/imgui/imgui.cpp
		Submodule/imgui/imgui_draw.cpp
		Submodule/imgui/imgui_tables.cpp
		Submodule/imgui/imgui_widgets.cpp
	)

	target_include_directories(imgui PUBLIC Submodule/imgui)
	target_compile_features(imgui PUBLIC cxx_std_17)

	if(MSVC)
		# Vendored code - don't hold it to our own warning level.
		target_compile_options(imgui PRIVATE /w)
		# Same reasoning as simdjson/fastgltf/zstd/meshoptimizer above - keep this target's
		# _ITERATOR_DEBUG_LEVEL in sync so the linker doesn't reject the mix.
		target_compile_definitions(imgui PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()

	set_target_properties(imgui PROPERTIES FOLDER ThirdParty)
endif()

## Recast & Detour - submodule, pinned to v1.6.0. Built from their sources rather than their own
## CMakeLists, which also build a demo, tests and install rules. Detour finds paths at runtime, so it's
## always built. Recast builds navmeshes, which only the editor does.
set(TYR_RECAST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Submodule/recastnavigation)
file(GLOB DETOUR_SRCS ${TYR_RECAST_DIR}/Detour/Source/*.cpp)
add_library(Detour STATIC ${DETOUR_SRCS})
target_include_directories(Detour PUBLIC ${TYR_RECAST_DIR}/Detour/Include)
set(TYR_RECAST_TARGETS Detour)

if (TYR_EDITOR_MODE)
	file(GLOB RECAST_SRCS ${TYR_RECAST_DIR}/Recast/Source/*.cpp)
	add_library(Recast STATIC ${RECAST_SRCS})
	target_include_directories(Recast PUBLIC ${TYR_RECAST_DIR}/Recast/Include)
	list(APPEND TYR_RECAST_TARGETS Recast)
endif()

foreach(RECAST_TARGET ${TYR_RECAST_TARGETS})
	if(MSVC)
		# Vendored code - don't hold it to our own warning level.
		target_compile_options(${RECAST_TARGET} PRIVATE /w)
		# Same reasoning as the libraries above - keep _ITERATOR_DEBUG_LEVEL in sync for the linker.
		target_compile_definitions(${RECAST_TARGET} PRIVATE _ITERATOR_DEBUG_LEVEL=0)
	endif()
	set_target_properties(${RECAST_TARGET} PROPERTIES FOLDER ThirdParty)
endforeach()

## FMOD - audio. Its licence doesn't allow sharing the SDK, so it's never committed: it lives in
## ThirdParty/Licensed/FMOD, which git ignores, copied there from the FMOD Engine installer's folder
## the first time CMake finds it. Without it, everything builds and runs without audio.
set(TYR_FMOD_INSTALL_DIR "C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows" CACHE PATH
	"Where the FMOD Engine installer put its SDK, copied from when ThirdParty/Licensed/FMOD is missing.")
set(TYR_FMOD_DIR ${TYR_THIRD_PARTY_DIR}/Licensed/FMOD)
if (WIN32 AND NOT EXISTS ${TYR_FMOD_DIR}/core/inc/fmod.h AND EXISTS ${TYR_FMOD_INSTALL_DIR}/api/core/inc/fmod.h)
	message(STATUS "Copying the FMOD SDK from ${TYR_FMOD_INSTALL_DIR}")
	foreach(FMOD_PART core studio)
		file(COPY ${TYR_FMOD_INSTALL_DIR}/api/${FMOD_PART}/inc DESTINATION ${TYR_FMOD_DIR}/${FMOD_PART})
		file(COPY ${TYR_FMOD_INSTALL_DIR}/api/${FMOD_PART}/lib/${TYR_ARCHITECTURE} DESTINATION ${TYR_FMOD_DIR}/${FMOD_PART}/lib)
	endforeach()
endif()

if (WIN32 AND EXISTS ${TYR_FMOD_DIR}/core/inc/fmod.h AND EXISTS ${TYR_FMOD_DIR}/studio/inc/fmod_studio.h)
	set(TYR_HAS_FMOD ON CACHE INTERNAL "")
	# The core and Studio libraries, as DLLs with import libraries.
	foreach(FMOD_LIB fmod fmodstudio)
		set(FMOD_PART core)
		if (FMOD_LIB STREQUAL "fmodstudio")
			set(FMOD_PART studio)
		endif()
		set(FMOD_LIB_DIR ${TYR_FMOD_DIR}/${FMOD_PART}/lib/${TYR_ARCHITECTURE})
		add_library(${FMOD_LIB} SHARED IMPORTED GLOBAL)
		set_target_properties(${FMOD_LIB} PROPERTIES
			IMPORTED_LOCATION ${FMOD_LIB_DIR}/${FMOD_LIB}${CMAKE_SHARED_LIBRARY_SUFFIX}
			IMPORTED_IMPLIB ${FMOD_LIB_DIR}/${FMOD_LIB}_vc${CMAKE_IMPORT_LIBRARY_SUFFIX}
			INTERFACE_INCLUDE_DIRECTORIES ${TYR_FMOD_DIR}/${FMOD_PART}/inc)
	endforeach()
else()
	set(TYR_HAS_FMOD OFF CACHE INTERNAL "")
	message(STATUS "FMOD wasn't found, so the engine builds without audio. Install the FMOD Engine SDK "
		"(fmod.com/download) or set TYR_FMOD_INSTALL_DIR to where it is, then configure again.")
endif()

## Nuklear - submodule, pinned to v4.13.3. Single header, no build of its own needed - always
## available (not gated on TYR_FINAL_MODE/TYR_EDITOR_MODE) since it's the always-on runtime GUI
## (in-game HUD/menu), used by both an editor "play in editor" preview and the final shipped
## game (see GUIModule.cpp).
add_library(nuklear INTERFACE)
target_include_directories(nuklear INTERFACE Submodule/nuklear)
set_target_properties(nuklear PROPERTIES FOLDER ThirdParty)

# An INTERFACE library with no sources gets no project of its own in Visual Studio - attaching
# the header via target_sources isn't enough for this generator to create one either, so this
# is purely to make nuklear.h browsable under the ThirdParty solution folder; it builds nothing
# and nothing links against it (the nuklear target above is still what does that).
add_custom_target(nuklear_ide SOURCES Submodule/nuklear/nuklear.h)
set_target_properties(nuklear_ide PROPERTIES FOLDER ThirdParty)
