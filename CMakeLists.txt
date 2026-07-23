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
	${GENERATED}/tables.c)

target_include_directories(theremini-protocol PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/protocol/include>
	$<INSTALL_INTERFACE:include>)

set_target_properties(theremini-protocol PROPERTIES
	VERSION ${PROJECT_VERSION}
	POSITION_INDEPENDENT_CODE ON)

find_library(M_LIB m)
if(M_LIB)
	target_link_libraries(theremini-protocol PRIVATE ${M_LIB})
endif()

install(TARGETS theremini-protocol)
install(DIRECTORY src/protocol/include/ DESTINATION include)

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
