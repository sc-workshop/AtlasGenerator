include(cmake/googletest.cmake)

set(TARGET "AtlasGeneratorTests")

set(SOURCES
    source/config_tests.cpp
    source/item_tests.cpp
    source/generator_tests.cpp
)

add_executable(${TARGET} ${SOURCES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
wk_project_setup(${TARGET})

target_link_libraries(${TARGET} PRIVATE
    GTest::gtest_main
    wk::atlasGenerator
)

set_target_properties(${TARGET} PROPERTIES
    FOLDER WorkshopSDK/Tests/
)

include(GoogleTest)
gtest_discover_tests(${TARGET})
