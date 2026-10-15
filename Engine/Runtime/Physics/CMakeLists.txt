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

# List arguments need to be quoted. Otherwise the first entry in the list will be passed only.'
add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(TyrantPhysics SHARED ${SRCS})
copy_binaries(TyrantPhysics ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantPhysics)

# Includes
target_include_directories(TyrantPhysics PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Physics>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Physics>)

# Defines
target_compile_definitions(TyrantPhysics PRIVATE
	-DTYR_PHYSICS_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG>
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries. Jolt is private, so nothing outside this library sees it.
target_link_libraries(TyrantPhysics PUBLIC TyrantCore)
target_link_libraries(TyrantPhysics PRIVATE Jolt)

# IDE specific
set_property(TARGET TyrantPhysics PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Physics
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantPhysics)
