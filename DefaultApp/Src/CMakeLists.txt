set(EXCLUDED_SRCS "dummy")

add_source_groups(SRCS "${EXCLUDED_SRCS}")

# Target
add_library(${TYR_APP_LIB_NAME} SHARED ${SRCS})
copy_binaries(${TYR_APP_LIB_NAME} ${PROJECT_SOURCE_DIR})

add_common_properties(${TYR_APP_LIB_NAME})

# Includes
target_include_directories(${TYR_APP_LIB_NAME} PRIVATE
	$<BUILD_INTERFACE:${TYR_APP_DIR}/Src>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/${TYR_APP_LIB_NAME}>)
	

# Defines
target_compile_definitions(${TYR_APP_LIB_NAME} PRIVATE 
	-DTYR_PROJECT_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)

# Libraries
target_link_libraries(${TYR_APP_LIB_NAME} PRIVATE TyrantEngine)

# IDE specific
set_property(TARGET ${TYR_APP_LIB_NAME} PROPERTY FOLDER Project)

# Install
install(
	DIRECTORY ${TYR_APP_LIB_NAME}
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(${TYR_APP_LIB_NAME})


