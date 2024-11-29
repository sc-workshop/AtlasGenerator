cmake_minimum_required(VERSION 3.18)

project(WorkshopAtlasGenerator)

option(BUILD_ATLAS_GENERATOR_CLI "Build Atlas Generator CLI" OFF)
option(BUILD_ATLAS_GENERATOR_WITH_IMAGE_CODECS "Build an Atlas Generator with ability to load sprites directly from files" OFF)


if (${BUILD_ATLAS_GENERATOR_CLI})
    # OpenCV additional things
    set(BUILD_LIST "core;imgproc;imgcodecs;highgui" CACHE INTERNAL "")
    add_subdirectory(atlas-generator)
    add_subdirectory(atlas-generator-cli)
else()
    add_subdirectory(atlas-generator)
endif()