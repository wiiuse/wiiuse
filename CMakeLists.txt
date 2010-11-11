# Full-featured Build System Sample
# CMake cross-platform build system recipe
# 2009 Ryan Pavlik <rpavlik@iastate.edu>
# http://academic.cleardefinition.com/
# Iowa State University HCI Graduate Program/VRAC

cmake_minimum_required(VERSION 2.6)

# Set package properties
project(WiiUse)

###
# Set up options
###

option(BUILD_EXAMPLE "Should we build the example app?" YES)
option(BUILD_EXAMPLE_SDL "Should we build the SDL-based example app?" YES)
option(INSTALL_EXAMPLES "Should we install the example apps?" YES)

###
# Perform build configuration of dependencies
###

# Locally-developed modules dist'ed with this app - always have this first.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(UseBackportedModules)

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

###
# Set packaging options (for CPack)
###

# Include the packaging system now that we have it all set up
include(CPack)

