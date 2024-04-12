include(cmake/Dependecies.cmake)

set(TARGET "AtlasGeneratorCLI")

set(SOURCES
    cli/main.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
sc_core_base_setup(${TARGET})

target_link_libraries(${TARGET} PUBLIC 
    AtlasGenerator
)

add_compile_options(
    $<$<AND:${SC_MSVC},${SC_DEBUG}>: /DEBUG:FASTLINK>
)
