set(OpenGL_GL_PREFERENCE GLVND)
find_package(SDL)
find_package(OpenGL)
find_package(GLUT)

if(SDL_FOUND AND OPENGL_FOUND AND GLUT_FOUND)
	include_directories(../src ${SDL_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLUT_INCLUDE_DIRS})

	if(WIN32)
		add_executable(wiiuseexample-sdl WIN32 sdl.c)
	else()
		add_executable(wiiuseexample-sdl sdl.c)
	endif()

	target_link_libraries(wiiuseexample-sdl
		wiiuse
		SDL::SDL
		OpenGL::GLU
		GLUT::GLUT)

	if(INSTALL_EXAMPLES)
		install(TARGETS wiiuseexample-sdl
			RUNTIME DESTINATION bin COMPONENT examples)
	endif()
endif()
