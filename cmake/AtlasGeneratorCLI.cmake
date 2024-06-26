include(cmake/Dependecies.cmake)
include(cmake/AtlasGenerator.cmake)

set(TARGET "AtlasGeneratorCLI")
set(BASE_TARGET "AtlasGenerator")

set(SOURCES
    cli/main.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
sc_core_base_setup(${TARGET})

target_include_directories(${BASE_TARGET} PUBLIC
	${OPENCV_MODULE_opencv_highgui_LOCATION}/include
)

target_link_libraries(${TARGET} PUBLIC 
	opencv_highgui
    AtlasGenerator
)

add_compile_options(
    $<$<AND:${SC_MSVC},${SC_DEBUG}>: /DEBUG:FASTLINK>
)
