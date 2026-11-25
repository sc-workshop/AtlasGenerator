include(cmake/google-benchmark.cmake)

set(TARGET "AtlasGeneratorBenchmarks")

set(SOURCES
    source/main.cpp
    source/dataset.h
    source/dataset.cpp
    source/generation_benchmark.cpp
    source/place_image_benchmark.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET})

target_link_libraries(${TARGET} PRIVATE
    benchmark::benchmark
    wk::atlasGenerator
)

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/Benchmarks/
)
