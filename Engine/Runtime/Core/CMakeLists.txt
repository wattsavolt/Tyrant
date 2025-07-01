include(CMakeDependentOption)

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
add_library(TyrantCore SHARED ${SRCS})
copy_binaries(TyrantCore ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantCore)

# Includes
target_include_directories(TyrantCore PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Core>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Core>)

# Defines
target_compile_definitions(TyrantCore PRIVATE 
	-DTYR_CORE_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)
	
if(TYR_EDITOR_MODE)
	target_compile_definitions(TyrantCore PUBLIC TYR_IS_EDITOR=1) 	
else()
	target_compile_definitions(TyrantCore PUBLIC TYR_IS_EDITOR=0) 
endif()

if(TYR_FINAL_MODE)
	target_compile_definitions(TyrantCore PUBLIC TYR_IS_FINAL=1) 
else()
	target_compile_definitions(TyrantCore PUBLIC TYR_IS_FINAL=0) 
endif()

if(WIN32)
	target_link_libraries(TyrantCore PRIVATE Rpcrt4)
endif()

option(TYR_USE_SIMD "If true, SIMD instructions are used in math types." ON)

if(TYR_USE_SIMD)
	target_compile_definitions(TyrantCore PUBLIC TYR_USE_SIMD=1) 	
else()
	target_compile_definitions(TyrantCore PUBLIC TYR_USE_SIMD=0) 
endif()

cmake_dependent_option(TYR_USE_AVX_INTRINSICS "If true, AVX SIMD instructions are used in math types." ON "TYR_USE_SIMD" ON)

if(TYR_USE_AVX_INTRINSICS)
	target_compile_definitions(TyrantCore PUBLIC TYR_AVX_INTRINSICS) 	
endif()

# IDE specific
set_property(TARGET TyrantCore PROPERTY FOLDER Runtime)

# Install
install(
	DIRECTORY Core
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantCore)

copy_thirdparty_binaries(TyrantCore)


