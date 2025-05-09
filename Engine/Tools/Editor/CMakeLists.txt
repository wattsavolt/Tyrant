set(EXCLUDED_SRCS 
	"Windows"
	"Unix"
	"Linux"
	"MacOS")

if(WIN32)
	list(REMOVE_ITEM EXCLUDED_SRCS "Windows")
endif()

if(UNIX)
	list(REMOVE_ITEM EXCLUDED_SRCS "Unix")

	if(LINUX)
		list(REMOVE_ITEM EXCLUDED_SRCS "Linux")
	elseif(APPLE)
		list(REMOVE_ITEM EXCLUDED_SRCS "MacOS")
	endif()
endif()

add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(TyrantEditor SHARED ${SRCS})
copy_binaries(TyrantEditor ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantEditor)

# Includes
target_include_directories(TyrantEditor PUBLIC
	$<BUILD_INTERFACE:${TYR_TOOLS_DIR}/Editor>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Editor>)
	

# Defines
target_compile_definitions(TyrantEditor PRIVATE 
	-DTYR_EDITOR_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries
target_link_libraries(TyrantEditor PRIVATE TyrantEngine)

# IDE specific
set_property(TARGET TyrantEditor PROPERTY FOLDER Tools)

# Install
install(
	DIRECTORY Editor
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantEditor)


