include(cmake/Dependencies.cmake)

set(TARGET "AtlasGenerator")

file(GLOB_RECURSE SOURCES source/*)

add_library(${TARGET} STATIC ${SOURCES})
add_library(wk::atlasGenerator ALIAS ${TARGET})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET}) 

if (${BUILD_ATLAS_GENERATOR_WITH_IMAGE_CODECS})
    wk_set_global(ATLAS_GENERATOR_WITH_IMAGE_CODECS 1)
endif()

# Public dependencies
target_link_libraries(${TARGET} PUBLIC 
    wk::core
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
)

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/
)