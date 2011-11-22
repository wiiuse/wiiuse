set(SOURCES
	classic.c
	dynamics.c
	events.c
	guitar_hero_3.c
	io.c
	ir.c
	nunchuk.c
	wiiuse.c
	wiiboard.c
	classic.h
	definitions.h
	dynamics.h
	events.h
	guitar_hero_3.h
	motion_plus.h
	motion_plus.c
	io.h
	ir.h
	nunchuk.h
	os.h
	util.c
	wiiuse_internal.h
	wiiboard.h)
set(API
	wiiuse.h)

if(WIN32)
	list(APPEND SOURCES io_win.c)
	set(CMAKE_DEBUG_POSTFIX _debug)
elseif(APPLE)
	list(APPEND SOURCES io_mac.h)
	list(APPEND SOURCES io_mac.m)
else()
	list(APPEND SOURCES io_nix.c)
endif()

if(MSVC)
	list(APPEND API wiiuse_msvcstdint.h)
endif()

add_definitions(-DWIIUSE_COMPILE_LIB)

add_library(wiiuse ${WIIUSE_LIB_TYPE} ${SOURCES} ${API})

if(WIN32)
	target_link_libraries(wiiuse ws2_32 setupapi ${WINHID_LIBRARIES})
elseif(LINUX)
	target_link_libraries(wiiuse m ${BLUEZ_LIBRARIES})
elseif(APPLE)
	find_library(IOBLUETOOTH_FRAMEWORK
		NAMES
		IOBluetooth)
	
	find_library(COREFOUNDATION_FRAMEWORK
		NAMES
		CoreFoundation)
		
	find_library(FOUNDATION_FRAMEWORK
		NAMES
		Foundation)
	target_link_libraries(wiiuse ${IOBLUETOOTH_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK} ${FOUNDATION_FRAMEWORK})
endif()

set_property(TARGET
	wiiuse
	PROPERTY
	PUBLIC_HEADER
	${API})

set_property(TARGET
	wiiuse
	PROPERTY
	COMPILE_DEFINITIONS_DEBUG
	WITH_WIIUSE_DEBUG)

install(TARGETS
	wiiuse
	RUNTIME
	DESTINATION
	bin
	COMPONENT
	runtime
	ARCHIVE
	DESTINATION
	lib
	COMPONENT
	development
	LIBRARY
	DESTINATION
	lib
	COMPONENT
	runtime
	PUBLIC_HEADER
	DESTINATION
	include
	COMPONENT
	development)
