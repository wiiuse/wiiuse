find_package(SDL)

if(SDL_FOUND)
	include_directories(../src ${SDL_INCLUDE_DIR})
	add_executable(example-sdl sdl.c)
	target_link_libraries(example-sdl wiiuse ${SDL_LIBRARY})
endif()