include(CheckCXXCompilerFlag)

if ("${PROJECT_SOURCE_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
	set(TYR_TOP_DIR True)
endif()

# Configuration types
if(NOT CMAKE_CONFIGURATION_TYPES) # Multiconfig generator?
	if(NOT CMAKE_BUILD_TYPE)
		message(STATUS "Defaulting to release build.")
		set_property(CACHE CMAKE_BUILD_TYPE PROPERTY VALUE "Release")
	endif()
endif()

# Includes required for various find_package calls
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${TYR_ENGINE_DIR}/CMake/Scripts")

if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
	set(TYR_64BIT true)
endif()

if(UNIX AND NOT APPLE)
	set(LINUX TRUE)
endif()

# Global compile & linker flags
### Target at least C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

## Remove /EHsc from CMAKE_CXX_FLAGS for MSVC to disable exceptions
if (MSVC)
	if(CMAKE_CXX_FLAGS MATCHES "/EHsc")
		string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	endif()
endif()

# Enable colored output
if (CMAKE_GENERATOR STREQUAL "Ninja")
	check_cxx_compiler_flag("-fdiagnostics-color=always" F_DIAGNOSTIC_COLOR_ALWAYS)
	if (F_DIAGNOSTIC_COLOR_ALWAYS)
		add_compile_options("-fdiagnostics-color=always")
	endif()
endif()

set(CMAKE_XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES")
set(CMAKE_FIND_FRAMEWORK "LAST")

# Output
if(TYR_64BIT)
	set(TYR_OUTPUT_DIR_PREFIX x64)
else()
	set(TYR_OUTPUT_DIR_PREFIX x86)
endif()

set(TYR_BINARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/bin/${TYR_OUTPUT_DIR_PREFIX})
set(TYR_LIBRARY_OUTPUT_DIR ${PROJECT_BINARY_DIR}/lib/${TYR_OUTPUT_DIR_PREFIX})

if (TYR_TOP_DIR)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${TYR_BINARY_OUTPUT_DIR}/Debug)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${TYR_BINARY_OUTPUT_DIR}/RelWithDebInfo)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${TYR_BINARY_OUTPUT_DIR}/MinSizeRel)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${TYR_BINARY_OUTPUT_DIR}/Release)

	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${TYR_BINARY_OUTPUT_DIR}/Debug)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${TYR_BINARY_OUTPUT_DIR}/RelWithDebInfo)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${TYR_BINARY_OUTPUT_DIR}/MinSizeRel)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${TYR_BINARY_OUTPUT_DIR}/Release)

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${TYR_LIBRARY_OUTPUT_DIR}/Debug)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${TYR_LIBRARY_OUTPUT_DIR}/RelWithDebInfo)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL ${TYR_LIBRARY_OUTPUT_DIR}/MinSizeRel)
	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${TYR_LIBRARY_OUTPUT_DIR}/Release)

	set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)
endif()

# Look for global/system dependencies
if (UNIX)
	# macOS
	if (CMAKE_SYSTEM_NAME STREQUAL Darwin)
		# Find tools used for stripping binaries
		find_program(DSYMUTIL_TOOL dsymutil)

		if (NOT DSYMUTIL_TOOL)
			message(FATAL_ERROR "Could not find 'dsymutil' tool.")
		endif()

		find_program(STRIP_TOOL strip)
		if (NOT STRIP_TOOL)
			message(FATAL_ERROR "Could not find 'strip' tool.")
		endif()

	# Linux
	else()
		# Find tools used for stripping binaries
	    find_program(OBJCOPY_TOOL objcopy)

	    if (NOT OBJCOPY_TOOL)
	        message(FATAL_ERROR "Could not find 'objcopy' tool.")
	    endif()

	endif()
endif()