include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Basic Types & Defintions
FetchContent_Declare(
    WorkshopCore
    GIT_REPOSITORY https://github.com/sc-workshop/Workshop-Core.git
    GIT_TAG main
)
FetchContent_MakeAvailable(WorkshopCore)

# Image Processing
include(cmake/opencv.cmake)

# 2D packaging
include(cmake/libnest2d.cmake)

# Intersection helper
include(cmake/clipper2.cmake)