cmake_minimum_required(VERSION 3.18)

project(WorkshopAtlasGenerator)

option(BUILD_ATLAS_GENERATOR_CLI "Build Atlas Generator CLI" OFF)
option(BUILD_ATLAS_GENERATOR_TESTS "Build Atlas Generator GoogleTest suite" OFF)
option(BUILD_ATLAS_GENERATOR_BENCHMARKS "Build Atlas Generator Google Benchmark suite" OFF)

if (${BUILD_ATLAS_GENERATOR_CLI})
    # OpenCV additional things
    set(BUILD_LIST "core;imgproc;imgcodecs;highgui" CACHE INTERNAL "")
    set(BUILD_ATLAS_GENERATOR_WITH_IMAGE_CODECS ON)
    add_subdirectory(atlas-generator)
    add_subdirectory(atlas-generator-cli)
else()
    add_subdirectory(atlas-generator)
endif()

if (${BUILD_ATLAS_GENERATOR_TESTS})
    enable_testing()
    add_subdirectory(tests)
endif()

if (${BUILD_ATLAS_GENERATOR_BENCHMARKS})
    add_subdirectory(benchmarks)
endif()