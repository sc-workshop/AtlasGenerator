cmake_minimum_required(VERSION 3.18)

project(ScAtlasGenerator)

include(cmake/AtlasGenerator.cmake)

option(SC_BUILD_ATLAS_GENERATOR_CLI "Build Atlas Generator CLI" OFF)

if (SC_BUILD_ATLAS_GENERATOR_CLI)
    include(cmake/AtlasGeneratorCLI.cmake)
endif()