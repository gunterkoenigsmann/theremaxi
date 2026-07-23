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

add_executable(test_value_export
	test_value_export.c
	${GENERATED}/golden_data.c)

target_include_directories(test_value_export PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_value_export PRIVATE theremini-protocol)

add_executable(test_preset_decode
	test_preset_decode.c
	${GENERATED}/golden_data.c)

target_include_directories(test_preset_decode PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_preset_decode PRIVATE theremini-protocol)

add_executable(test_sysex
	test_sysex.c
	${GENERATED}/golden_data.c)

target_include_directories(test_sysex PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(test_sysex PRIVATE theremini-protocol)

add_test(NAME value_export COMMAND test_value_export)
add_test(NAME preset_decode COMMAND test_preset_decode)
add_test(NAME sysex COMMAND test_sysex)
