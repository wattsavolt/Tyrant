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

# The FMOD backend is only built on machines that have the SDK (see ThirdParty/CMakeLists.txt).
# Without it, the library still builds, with a backend that plays nothing.
if(NOT TYR_HAS_FMOD)
	list(APPEND EXCLUDED_SRCS "FMOD")
endif()

# List arguments need to be quoted. Otherwise the first entry in the list will be passed only.'
add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(TyrantAudio SHARED ${SRCS})
copy_binaries(TyrantAudio ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantAudio)

# Includes
target_include_directories(TyrantAudio PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Audio>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Audio>)

# Defines. Whether FMOD is there stays private, so nothing outside this library depends on it.
target_compile_definitions(TyrantAudio PRIVATE
	-DTYR_AUDIO_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG>
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

if(TYR_HAS_FMOD)
	target_compile_definitions(TyrantAudio PRIVATE TYR_HAS_FMOD=1)
	target_link_libraries(TyrantAudio PRIVATE fmod fmodstudio)
else()
	target_compile_definitions(TyrantAudio PRIVATE TYR_HAS_FMOD=0)
endif()

# Libraries
target_link_libraries(TyrantAudio PUBLIC TyrantCore)

# IDE specific
set_property(TARGET TyrantAudio PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Audio
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
	PATTERN "FMOD" EXCLUDE
)

install_tyr_target(TyrantAudio)
