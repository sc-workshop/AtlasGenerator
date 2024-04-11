include(cmake/Dependecies.cmake)

set(TARGET "AtlasGenerator")

file(GLOB_RECURSE SOURCES
    "source/*"
)

file(GLOB_RECURSE HEADERS
    "include/*"
)

add_library(${TARGET} STATIC ${SOURCES} ${HEADERS})
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES} ${HEADERS})
sc_core_base_setup(${TARGET}) 

target_link_libraries(${TARGET} PUBLIC 
    SupercellCore
    libnest2d
    opencv_core opencv_imgproc opencv_imgcodecs opencv_highgui
)

target_include_directories(${TARGET}
    PUBLIC
    "include/"
    ${OPENCV_CONFIG_FILE_INCLUDE_DIR}
    ${opencv_SOURCE_DIR}/include
    ${OPENCV_MODULE_opencv_core_LOCATION}/include
    ${OPENCV_MODULE_opencv_imgproc_LOCATION}/include
    ${OPENCV_MODULE_opencv_imgcodecs_LOCATION}/include
    ${OPENCV_MODULE_opencv_highgui_LOCATION}/include
)
