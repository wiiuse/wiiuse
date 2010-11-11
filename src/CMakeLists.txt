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
	io.h
	ir.h
	nunchuk.h
	os.h
	wiiuse_internal.h
	wiiboard.h)
set(API
	wiiuse.h)
	
if(WIN32)
	list(APPEND SOURCES io_win.c)
	list(APPEND API ../msvc-stdint/stdint.h)
else()
	list(APPEND SOURCES io_nix.c)
endif()



add_library(wiiuse SHARED ${SOURCES} ${API})
if(WIN32)
	target_link_libraries(wiiuse Ws2_32.lib hid.lib setupapi.lib)
else()
	target_link_libraries(wiiuse m ${BLUEZ_LIBRARIES})
endif()

set_property(TARGET
	wiiuse
	PROPERTY
	PUBLIC_HEADER
	${API})

install(TARGETS
	wiiuse
	RUNTIME
	DESTINATION
	bin
	ARCHIVE
	DESTINATION
	lib
	LIBRARY
	DESTINATION
	lib
	PUBLIC_HEADER
	DESTINATION
	include)
