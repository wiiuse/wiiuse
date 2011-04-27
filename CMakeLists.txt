# 2009-2011 Ryan Pavlik <rpavlik@iastate.edu>
# http://academic.cleardefinition.com/
# Iowa State University HCI Graduate Program/VRAC

cmake_minimum_required(VERSION 2.6.2)

# Set package properties
project(WiiUse)

###
# Set up options
###

include("./ParseVersion.cmake")
message(STATUS "Configuring WiiUse version ${CPACK_PACKAGE_VERSION}")

option(BUILD_EXAMPLE "Should we build the example app?" YES)
option(BUILD_EXAMPLE_SDL "Should we build the SDL-based example app?" YES)
option(INSTALL_EXAMPLES "Should we install the example apps?" YES)

option(CPACK_MONOLITHIC_INSTALL "Only produce a single component installer, rather than multi-component." NO)

###
# Perform build configuration of dependencies
###

# Locally-developed modules dist'ed with this app - always have this first.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(UseBackportedModules)
include(DoxygenTargets)

add_definitions(-DWIIUSE_COMPILE_LIB)

if(NOT WIN32 AND NOT APPLE)
	set(LINUX YES)
	find_package(Bluez REQUIRED)
	include_directories(${BLUEZ_INCLUDE_DIRS})
else()
	set(LINUX NO)
endif()

if(WIN32)
	find_package(WinHID REQUIRED)
	include_directories(${WINHID_INCLUDE_DIRS})
endif()

###
# Build the project
###

if(MSVC)
	include_directories("${CMAKE_CURRENT_SOURCE_DIR}/msvc-stdint")
endif()

# The lib is in the "src" subdirectory
add_subdirectory(src)

# Example apps
if(BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(BUILD_EXAMPLE_SDL)
    add_subdirectory(example-sdl)
endif()

# Documentation
add_doxygen(Doxyfile)

###
# Set packaging options (for CPack)
###

if(WIN32)
	set(DOC_DIR .)
else()
	set(DOC_DIR share/doc/wiiuse)
endif()

install(FILES
	CHANGELOG
	LICENSE
	README
	DESTINATION ${DOC_DIR} COMPONENT docs)

if(INSTALL_EXAMPLES)
	install(FILES example/example.c
		DESTINATION ${DOC_DIR}/example COMPONENT examples)
	install(FILES example-sdl/sdl.c
		DESTINATION ${DOC_DIR}/example-sdl COMPONENT examples)
endif()

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README")

include(GetCompilerInfoString)
get_compiler_info_string(_compiler)

set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${_compiler}")

# Include the packaging system now that we have it all set up
include(CPack)

cpack_add_component(docs HIDDEN)

cpack_add_component(development
	DISPLAY_NAME "Development Files")

cpack_add_component(examples
	DISPLAY_NAME "Examples")

cpack_add_component(runtime
	DISPLAY_NAME "Runtime Library"
	REQUIRED)
