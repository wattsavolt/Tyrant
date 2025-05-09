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
set_shader_config("${SRCS}")

# Target
add_library(TyrantRenderer SHARED ${SRCS})
copy_binaries(TyrantRenderer ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantRenderer)


# Includes
target_include_directories(TyrantRenderer PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Renderer>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Renderer>)
	

# Defines
target_compile_definitions(TyrantRenderer PRIVATE 
	-DTYR_RENDERER_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries
target_link_libraries(TyrantRenderer PUBLIC TyrantGraphics)


# IDE specific
set_property(TARGET TyrantRenderer PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Renderer
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantRenderer)


