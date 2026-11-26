include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Basic Types & Defintions
FetchContent_Declare(
    WorkshopCore
    GIT_REPOSITORY https://github.com/sc-workshop/Workshop-Core.git
    GIT_TAG main
)
FetchContent_MakeAvailable(WorkshopCore)

# OpenCV is optional and used for image codecs.
if (${BUILD_ATLAS_GENERATOR_WITH_IMAGE_CODECS})
    # Image Processing
    include(cmake/opencv.cmake)
endif()

# Geometry helpers
include(cmake/clipper2.cmake)

# Minimizer backend
include(cmake/nlopt.cmake)
