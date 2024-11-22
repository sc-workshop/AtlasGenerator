
set(TARGET "AtlasGeneratorCLI")
set(BASE_TARGET "AtlasGenerator")

set(SOURCES
    source/main.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET})

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

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/CLI
)
