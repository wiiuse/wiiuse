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
else()
	list(APPEND SOURCES io_nix.c)
endif()

if(MSVC)
	list(APPEND API wiiuse_msvcstdint.h)
endif()

add_library(wiiuse SHARED ${SOURCES} ${API})

if(WIN32)
	target_link_libraries(wiiuse ws2_32 setupapi ${WINHID_LIBRARIES})
elseif(LINUX)
	target_link_libraries(wiiuse m ${BLUEZ_LIBRARIES})
endif()

set_property(TARGET
	wiiuse
	PROPERTY
	PUBLIC_HEADER
	${API})

#set_property(TARGET
#	wiiuse
#	PROPERTY
#	COMPILE_DEFINITIONS_DEBUG
#	WITH_WIIUSE_DEBUG)

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
