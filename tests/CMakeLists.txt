add_custom_command(
	OUTPUT ${GENERATED}/golden_data.c
	COMMAND ${PERL_EXECUTABLE}
	        ${CMAKE_SOURCE_DIR}/tools/generate-c-golden.pl
	        ${CMAKE_SOURCE_DIR}/protocol/golden.json
	        ${GENERATED}/golden_data.c
	DEPENDS ${CMAKE_SOURCE_DIR}/tools/generate-c-golden.pl
	        ${CMAKE_SOURCE_DIR}/protocol/golden.json
	COMMENT "Generating the golden vectors from protocol/golden.json"
	VERBATIM)

# Each test replays part of protocol/golden.json through the library.
foreach(name value_export preset_decode sysex message)
	add_executable(test_${name} test_${name}.c ${GENERATED}/golden_data.c)
	target_include_directories(test_${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(test_${name} PRIVATE theremini-protocol)
	add_test(NAME ${name} COMMAND test_${name})
endforeach()
