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

# The LV2 plugin, driven through its own descriptor - built only where LV2 is.
if(THEREMINI_HAVE_LV2)
	add_executable(test_lv2
		test_lv2.c
		${CMAKE_SOURCE_DIR}/src/lv2/theremini.c
		${GENERATED}/lv2_ports.h)
	target_include_directories(test_lv2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GENERATED})
	target_link_libraries(test_lv2 PRIVATE theremini-protocol PkgConfig::LV2)
	add_test(NAME lv2 COMMAND test_lv2)

	# structural check of the generated TTL against the LV2 ontology.
	# lv2_validate is a wrapper around sord_validate, so both must be present -
	# otherwise the check would fail for a missing tool rather than a bad TTL.
	find_program(LV2_VALIDATE lv2_validate)
	find_program(SORD_VALIDATE sord_validate)
	if(LV2_VALIDATE AND SORD_VALIDATE)
		add_test(NAME lv2_ttl
			COMMAND ${LV2_VALIDATE}
			        ${CMAKE_BINARY_DIR}/theremini.lv2/manifest.ttl
			        ${CMAKE_BINARY_DIR}/theremini.lv2/theremini.ttl)
	endif()
endif()
