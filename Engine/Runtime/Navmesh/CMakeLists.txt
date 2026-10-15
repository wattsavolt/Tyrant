set(EXCLUDED_SRCS
	"Win32"
	"Unix"
	"Linux"
	"MacOS")

if(WIN32)
	list(REMOVE_ITEM EXCLUDED_SRCS "Win32")
endif()

if(UNIX)
	list(REMOVE_ITEM EXCLUDED_SRCS "Unix")

	if(LINUX)
		list(REMOVE_ITEM EXCLUDED_SRCS "Linux")
	elseif(APPLE)
		list(REMOVE_ITEM EXCLUDED_SRCS "MacOS")
	endif()
endif()

# Building navmeshes is only done in the editor, so the builder is left out of other builds.
if(NOT TYR_EDITOR_MODE)
	list(APPEND EXCLUDED_SRCS "Build")
endif()

# List arguments need to be quoted. Otherwise the first entry in the list will be passed only.'
add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(TyrantNavmesh SHARED ${SRCS})
copy_binaries(TyrantNavmesh ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantNavmesh)

# Includes
target_include_directories(TyrantNavmesh PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Navmesh>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Navmesh>)

# Defines
target_compile_definitions(TyrantNavmesh PRIVATE
	-DTYR_NAVMESH_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG>
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries. Recast and Detour are private, so nothing outside this library sees them.
target_link_libraries(TyrantNavmesh PUBLIC TyrantCore)
target_link_libraries(TyrantNavmesh PRIVATE Detour)
if(TYR_EDITOR_MODE)
	target_link_libraries(TyrantNavmesh PRIVATE Recast)
endif()

# IDE specific
set_property(TARGET TyrantNavmesh PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Navmesh
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantNavmesh)
