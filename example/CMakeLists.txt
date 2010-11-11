include_directories(../src)
add_executable(example example.c)
target_link_libraries(example wiiuse)

if(INSTALL_EXAMPLES)
	install(TARGETS example
		RUNTIME DESTINATION bin COMPONENT examples)
endif()