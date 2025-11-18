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
if(WIN32)
	add_executable(TyrantApp WIN32 ${SRCS})
else()
	add_executable(TyrantApp ${SRCS})
endif()

copy_binaries(TyrantApp ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantApp)

# Includes
target_include_directories(TyrantApp PRIVATE
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/App>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/App>)
	

# Defines
target_compile_definitions(TyrantApp PRIVATE 
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)
	
target_compile_definitions(TyrantApp PUBLIC TYR_APP_LIB_NAME=${TYR_APP_LIB_NAME}) 	

# Libraries
target_link_libraries(TyrantApp PRIVATE TyrantEngine)
if(TYR_EDITOR_MODE)
target_link_libraries(TyrantApp PRIVATE TyrantEditor)
endif()

# IDE specific
set_property(TARGET TyrantApp PROPERTY FOLDER Runtime)

# Make name of executable the app name
if(TYR_EDITOR_MODE)
	set_property(TARGET TyrantApp PROPERTY OUTPUT_NAME ${TYR_APP_NAME}_Editor)
else()
	set_property(TARGET TyrantApp PROPERTY OUTPUT_NAME ${TYR_APP_NAME})
endif()

# Install
install(
	DIRECTORY App
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantApp)


