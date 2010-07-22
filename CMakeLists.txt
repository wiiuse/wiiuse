# Full-featured Build System Sample
# CMake cross-platform build system recipe
# 2009 Ryan Pavlik <rpavlik@iastate.edu>
# http://academic.cleardefinition.com/
# Iowa State University HCI Graduate Program/VRAC

cmake_minimum_required(VERSION 2.6)

# Set package properties
project(wiiuse)

###
# Set up options
###

# See the cmake docs about the "option" command if you have build-time configuration,
# such as ifdefs, etc.

###
# Perform build configuration of dependencies
###

# Locally-developed modules dist'ed with this app - always have this first.
#list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
#include(UseBackportedModules)

###
# Build the project
###

# The app is in the "src" subdirectory
add_subdirectory(src)
# Add any more subdirectories to recurse into here.
	
###
# Other things you might like to do
###

# Add nightly builds/dashboard support
#include(CreateDashboardScripts)
#create_dashboard_scripts(DashboardBuildInitialCache.cmake.in)

###
# Set packaging options (for CPack)
###

# Include the packaging system now that we have it all set up
include(CPack)

