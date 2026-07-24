cmake_minimum_required(VERSION 3.20)

project(theremini
	VERSION 0.1.0
	DESCRIPTION "Protocol and device libraries for the Moog Theremini"
	LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(THEREMINI_BUILD_TESTS "Build the tests" ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Perl generates the tables from protocol/tables.json. It is already required to
# run the reference implementation, so this adds no new build dependency, and
# generating means the C copy can never drift from the JSON.
find_package(Perl REQUIRED)

set(GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED})

add_custom_command(
	OUTPUT ${GENERATED}/tables.c
	COMMAND ${PERL_EXECUTABLE}
	        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-c-tables.pl
	        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/tables.json
	        ${GENERATED}/tables.c
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-c-tables.pl
	        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/tables.json
	COMMENT "Generating the parameter table from protocol/tables.json"
	VERBATIM)

add_library(theremini-protocol
	src/protocol/value.c
	src/protocol/preset.c
	src/protocol/sysex.c
	src/protocol/message.c
	${GENERATED}/tables.c)

target_include_directories(theremini-protocol PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/include>
	$<INSTALL_INTERFACE:include>)

# internal.h and the generated tables.c that includes it
target_include_directories(theremini-protocol PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/src/protocol)

set_target_properties(theremini-protocol PROPERTIES
	VERSION ${PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE ON)

find_library(M_LIB m)
if(M_LIB)
	target_link_libraries(theremini-protocol PRIVATE ${M_LIB})
endif()

install(TARGETS theremini-protocol)
install(DIRECTORY src/protocol/include/ DESTINATION include)

# ---------------------------------------------------------------- LV2 plugin

# The port list and the C port table are both generated from tables.json so
# they cannot drift from each other or from the parameters.
add_custom_command(
	OUTPUT ${GENERATED}/theremini.ttl ${GENERATED}/lv2_ports.h
	COMMAND ${PERL_EXECUTABLE}
	        ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-lv2.pl
	        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/tables.json
	        ${GENERATED}/theremini.ttl
	        ${GENERATED}/lv2_ports.h
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-lv2.pl
	        ${CMAKE_CURRENT_SOURCE_DIR}/protocol/tables.json
	COMMENT "Generating the LV2 port description from protocol/tables.json"
	VERBATIM)

find_package(PkgConfig)
set(THEREMINI_HAVE_LV2 FALSE)
if(PkgConfig_FOUND)
	pkg_check_modules(LV2 IMPORTED_TARGET lv2)
endif()

if(LV2_FOUND)
	set(THEREMINI_HAVE_LV2 TRUE)
	set(BUNDLE ${CMAKE_CURRENT_BINARY_DIR}/theremini.lv2)
	file(MAKE_DIRECTORY ${BUNDLE})

	configure_file(src/lv2/manifest.ttl.in ${BUNDLE}/manifest.ttl @ONLY)

	# copy the generated port description into the bundle
	add_custom_command(
		OUTPUT ${BUNDLE}/theremini.ttl
		COMMAND ${CMAKE_COMMAND} -E copy ${GENERATED}/theremini.ttl ${BUNDLE}/theremini.ttl
		DEPENDS ${GENERATED}/theremini.ttl
		VERBATIM)
	add_custom_target(lv2_ttl DEPENDS ${BUNDLE}/theremini.ttl)

	add_library(theremini_lv2 MODULE
		src/lv2/theremini.c
		${GENERATED}/lv2_ports.h)
	add_dependencies(theremini_lv2 lv2_ttl)
	set_target_properties(theremini_lv2 PROPERTIES
		OUTPUT_NAME theremini
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY ${BUNDLE})
	target_include_directories(theremini_lv2 PRIVATE ${GENERATED})
	target_link_libraries(theremini_lv2 PRIVATE theremini-protocol PkgConfig::LV2)

	# install into the user or system LV2 path
	install(DIRECTORY ${BUNDLE} DESTINATION lib/lv2)
endif()

if(THEREMINI_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

# Optional API docs: cmake --build build --target docs
find_package(Doxygen)
if(DOXYGEN_FOUND)
	set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs)
	configure_file(Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
	add_custom_target(docs
		COMMAND Doxygen::doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		COMMENT "Generating API documentation with Doxygen"
		VERBATIM)
endif()
