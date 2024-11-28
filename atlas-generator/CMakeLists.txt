include(cmake/Dependencies.cmake)

set(TARGET "AtlasGenerator")

file(GLOB_RECURSE SOURCES source/*)

add_library(${TARGET} STATIC ${SOURCES})
add_library(wk::atlasGenerator ALIAS ${TARGET})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET}) 

# Public dependencies
target_link_libraries(${TARGET} PUBLIC 
    wk::core
    opencv_core opencv_imgproc opencv_imgcodecs
)

# Private dependencies
target_link_libraries(${TARGET} PUBLIC 
    libnest2d
    Clipper2
)

target_include_directories(${TARGET}
    PUBLIC
    "source/"
    ${OPENCV_CONFIG_FILE_INCLUDE_DIR}
    ${opencv_SOURCE_DIR}/include
    ${OPENCV_MODULE_opencv_core_LOCATION}/include
    ${OPENCV_MODULE_opencv_imgproc_LOCATION}/include
    ${OPENCV_MODULE_opencv_imgcodecs_LOCATION}/include
)

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/
)

#add_custom_command(TARGET ${TARGET} POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:${TARGET}> $<TARGET_FILE_DIR:${TARGET}>
#    COMMAND_EXPAND_LISTS
#)

if (BUILD_SHARED_LIBS)
    
endif()