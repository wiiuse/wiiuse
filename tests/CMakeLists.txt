# libcheck is dev/test-only infrastructure, not a library dependency - never
# make the top-level configure fail just because it (or pkg-config itself)
# isn't installed. Platforms/developers without it simply don't get the test
# targets, rather than being unable to build the library at all.
find_package(PkgConfig)
if(PkgConfig_FOUND)
	pkg_check_modules(CHECK IMPORTED_TARGET check)
endif()

if(TARGET PkgConfig::CHECK)
	add_executable(test_invariant_wiiuse test_invariant_wiiuse.c)
	target_include_directories(test_invariant_wiiuse PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(test_invariant_wiiuse PRIVATE wiiuse PkgConfig::CHECK)
	add_test(NAME invariant_wiiuse COMMAND test_invariant_wiiuse)

	add_executable(test_accel_wiimote test_accel_wiimote.c)
	target_include_directories(test_accel_wiimote PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(test_accel_wiimote PRIVATE wiiuse PkgConfig::CHECK)
	add_test(NAME accel_wiimote COMMAND test_accel_wiimote)

	add_executable(test_accel_nunchuk test_accel_nunchuk.c)
	target_include_directories(test_accel_nunchuk PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(test_accel_nunchuk PRIVATE wiiuse PkgConfig::CHECK)
	add_test(NAME accel_nunchuk COMMAND test_accel_nunchuk)

	add_executable(test_events_accel_threshold test_events_accel_threshold.c)
	target_include_directories(test_events_accel_threshold PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_link_libraries(test_events_accel_threshold PRIVATE wiiuse PkgConfig::CHECK)
	add_test(NAME events_accel_threshold COMMAND test_events_accel_threshold)
else()
	message(STATUS "libcheck not found via pkg-config - skipping test suite (BUILD_TESTING has no effect here)")
endif()
