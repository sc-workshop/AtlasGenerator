cmake_minimum_required(VERSION 3.18)

project(ScAtlasGenerator)

option(SC_BUILD_ATLAS_GENERATOR_CLI "Build Atlas Generator CLI" OFF)

if (${SC_BUILD_ATLAS_GENERATOR_CLI})
    # OpenCV additional things
    set(BUILD_LIST CACHE INTERNAL ""
        core
        imgproc
        imgcodecs
        highgui
    )
    
    add_subdirectory(atlas-generator)
    add_subdirectory(atlas-generator-cli)
else()
    add_subdirectory(atlas-generator)
endif()