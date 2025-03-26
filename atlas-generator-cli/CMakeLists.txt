
set(TARGET "AtlasGeneratorCLI")

set(SOURCES
    source/main.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET})

target_link_libraries(${TARGET} PUBLIC 
    AtlasGenerator
)

add_compile_options(
    $<$<AND:${SC_MSVC},${SC_DEBUG}>: /DEBUG:FASTLINK>
)

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/CLI
)

foreach(lib IN LISTS BUILD_LIST)
    target_link_libraries(${TARGET} PUBLIC 
        opencv_${lib}
    )

    target_include_directories(${TARGET}
        PUBLIC
        ${OPENCV_MODULE_opencv_${lib}_LOCATION}/include
    )
endforeach()

if (${BUILD_SHARED_LIBS})
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>
    COMMAND_EXPAND_LISTS
)
endif()