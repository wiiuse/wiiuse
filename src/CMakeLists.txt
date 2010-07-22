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

if(WIN32)
	list(APPEND SOURCES io_win.c)
else()
	list(APPEND SOURCES io_nix.c)
endif()

set(API
	wiiuse.h)

add_library(wiiuse SHARED ${SOURCES} ${API})
target_link_libraries(wiiuse m bluetooth)

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
	LIBRARY
	DESTINATION
	lib
	PUBLIC_HEADER
	DESTINATION
	include)
