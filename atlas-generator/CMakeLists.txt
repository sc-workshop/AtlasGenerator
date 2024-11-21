include(cmake/Dependencies.cmake)

set(TARGET "AtlasGenerator")

file(GLOB_RECURSE SOURCES source/*)

add_library(${TARGET} STATIC ${SOURCES})
add_library(wave::atlasGenerator ALIAS ${TARGET})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
sc_project_setup(${TARGET}) 

target_link_libraries(${TARGET} PUBLIC 
    supercell::core
    libnest2d
    opencv_core opencv_imgproc opencv_imgcodecs 
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
