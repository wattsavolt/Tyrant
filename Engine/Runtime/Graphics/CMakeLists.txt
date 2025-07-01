set(EXCLUDED_SRCS 
	"Win32"
	"Unix"
	"Linux"
	"MacOS"
	"ThirdParty"
)

message(STATUS "Using module to find Vulkan")
find_package(Vulkan)


if(NOT Vulkan_FOUND)
	list(APPEND EXCLUDED_SRCS "Vulkan")
endif()	


# Private third-party includes and libraries
set(PRIVATE_INC_DIRS "${TYR_THIRD_PARTY_HEADER_LIBS_DIR}/DirectXShaderCompiler/inc")
set(PRIVATE_LIBS "")

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
add_library(TyrantGraphics SHARED ${SRCS})
copy_binaries(TyrantGraphics ${PROJECT_SOURCE_DIR})

add_common_properties(TyrantGraphics)

# Includes
target_include_directories(TyrantGraphics PUBLIC
	$<BUILD_INTERFACE:${TYR_RUNTIME_DIR}/Graphics>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include/Graphics>)
	

# Defines
target_compile_definitions(TyrantGraphics PRIVATE 
	-DTYR_GRAPHICS_EXPORTS
	$<$<CONFIG:Debug>:TYR_CONFIG=TYR_CONFIG_DEBUG> 
	$<$<CONFIG:RelWithDebInfo>:TYR_CONFIG=TYR_CONFIG_RELWITHDEBINFO>
	$<$<CONFIG:MinSizeRel>:TYR_CONFIG=TYR_CONFIG_MINSIZEREL>
	$<$<CONFIG:Release>:TYR_CONFIG=TYR_CONFIG_RELEASE>)


# Libraries
target_link_libraries(TyrantGraphics PUBLIC TyrantCore)


# TODO: Disable for phones (if ever supported) to use render passes on Vulkan.
target_compile_definitions(TyrantGraphics PUBLIC TYR_USE_DYNAMIC_RENDERING=1) 

if(Vulkan_FOUND)
	target_compile_definitions(TyrantGraphics PUBLIC TYR_VULKAN_AVAILABLE=1) 
endif()	


# The Vulkan_ variables are built into CMAKE after version 3.7. Not supported on Apple yet.
if(Vulkan_FOUND)
	list(APPEND PRIVATE_INC_DIRS ${Vulkan_INCLUDE_DIRS})
	list(APPEND PRIVATE_LIBS ${Vulkan_LIBRARIES})

	if(WIN32)
		target_compile_definitions(TyrantGraphics PUBLIC VK_USE_PLATFORM_WIN32_KHR) 	
	endif()
	# TODO: Check how to tell if it's Android.'
	#if(ANDROID)
		#target_compile_definitions(TyrantGraphics PUBLIC VK_USE_PLATFORM_ANDROID_KHR) 	
	#endif()
	# TODO: Need to distinguish between MACOS and IOS in the future. Vulkan has different includes for each.
	if(APPLE)
		target_compile_definitions(TyrantGraphics PUBLIC VK_USE_PLATFORM_MACOS_MVK) 	
	endif()
	# TODO: Add more later

	#add_custom_command(TARGET TyrantGraphics
		#POST_BUILD
		#COMMAND ${CMAKE_COMMAND} -E copy mylib.dll ${TyrantGraphics_BINARY_DIR}
	#)
endif()	

target_include_directories(TyrantGraphics PRIVATE ${PRIVATE_INC_DIRS})

target_link_libraries(TyrantGraphics PRIVATE ${PRIVATE_LIBS})

# Vulkan_GLSLC_EXECUTABLE - the path to the GLSL SPIR-V compiler

# IDE specific
set_property(TARGET TyrantGraphics PROPERTY FOLDER Runtime)


# Target link directories

# Install
install(
	DIRECTORY Graphics
	DESTINATION include
	FILES_MATCHING 
	PATTERN "*.h"
	PATTERN "*.hpp"
	PATTERN "Private" EXCLUDE
)

install_tyr_target(TyrantGraphics)



