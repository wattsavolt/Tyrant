# Add below later. nvtt for nvidia texture compression and snappy for compression/decompression
# Packages
#find_package(snappy REQUIRED)
#find_package(nvtt REQUIRED)

if(LINUX)
	find_package(X11 REQUIRED)
	find_package(LibUUID REQUIRED)
	find_package(LibICU REQUIRED)

	if(NOT X11_Xcursor_FOUND)
		message(FATAL_ERROR "Could not find XCursor library.")
	endif()

	if(NOT X11_Xrandr_FOUND)
		message(FATAL_ERROR "Could not find XRandR library.")
	endif()

	if(NOT X11_Xi_FOUND)
		message(FATAL_ERROR "Could not find Xi (XInput) library.")
	endif()
elseif(APPLE)
	find_package(LibUUID REQUIRED)
endif()

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

add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(TyrantEngine SHARED ${SRCS})
copy_binaries(TyrantEngine ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantEngine)

# Includes
target_include_directories(TyrantEngine PUBLIC
	$<BUILD_INTERFACE:${TYR_THIRD_PARTY_INCLUDE_DIR}>
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Engine>
	$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/Generated/Engine>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Engine>)
	

# Defines
target_compile_definitions(TyrantEngine PRIVATE 
	-DTYR_ENGINE_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries
target_link_libraries(TyrantEngine PUBLIC TyrantCore)

target_link_libraries(TyrantEngine PUBLIC TyrantRenderer)

## External lib: NVTT
#target_link_libraries(TyrantEngine PRIVATE ${nvtt_LIBRARIES})	

## External lib: Snappy
#target_link_libraries(TyrantEngine PRIVATE ${snappy_LIBRARIES})

if(LINUX)
	## External lib: X11, LibUUID, LibICU
	target_link_libraries(TyrantEngine PUBLIC ${X11_LIBRARIES} ${X11_Xcursor_LIB} ${X11_Xrandr_LIB} ${X11_Xi_LIB})
	target_link_libraries(TyrantEngine PRIVATE ${LibUUID_LIBRARIES})
	target_link_libraries(TyrantEngine PRIVATE ${LibICU_LIBRARIES})
endif()

if(APPLE)
	## External lib: LibUUID
	target_link_libraries(TyrantEngine PRIVATE ${LibUUID_LIBRARIES})
endif()

## OS libs
if(WIN32)
	target_link_libraries(TyrantEngine PRIVATE Winmm dinput8 xinput9_1_0 dxguid.lib)
	target_link_libraries(TyrantEngine PRIVATE DbgHelp IPHLPApi Rpcrt4)
elseif(LINUX)
	target_link_libraries(TyrantEngine PRIVATE dl pthread)
elseif(APPLE) # MacOS
	target_link_framework(TyrantEngine CoreServices)
	target_link_framework(TyrantEngine IOKit)
	target_link_framework(TyrantEngine AppKit)
	target_link_framework(TyrantEngine Carbon)
endif()

# IDE specific
set_property(TARGET TyrantEngine PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Engine
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install(
	DIRECTORY ${PROJECT_BINARY_DIR}/Generated/Engine
	DESTINATION include
	FILES_MATCHING
	PATTERN "*.h"
)

install_tyr_target(TyrantEngine)


