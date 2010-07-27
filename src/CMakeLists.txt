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
elseif(APPLE)
	list(APPEND SOURCES io_mac.c)
else()
	list(APPEND SOURCES io_nix.c)
endif()

set(API
	wiiuse.h)

add_library(wiiuse SHARED ${SOURCES} ${API})
if(WIN32)
	# TODO
elseif(APPLE)
	find_library(IOBLUETOOTH_FRAMEWORK
		NAMES
		IOBluetooth)
	target_link_libraries(wiiuse ${IOBLUETOOTH_FRAMEWORK})
else()
	target_link_libraries(wiiuse m bluetooth)
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
	LIBRARY
	DESTINATION
	lib
	PUBLIC_HEADER
	DESTINATION
	include)
