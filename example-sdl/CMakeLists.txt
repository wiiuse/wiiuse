find_package(SDL)
find_package(OpenGL)
find_package(GLUT)

if(SDL_FOUND)
	include_directories(../src ${SDL_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIRS})
	add_executable(example-sdl sdl.c)
	target_link_libraries(example-sdl wiiuse ${SDL_LIBRARY} ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
endif()
