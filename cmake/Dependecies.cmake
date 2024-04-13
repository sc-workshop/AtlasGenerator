include(FetchContent)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# Basic Types & Defintions
FetchContent_Declare(
    SupercellCore
    GIT_REPOSITORY https://github.com/sc-workshop/SC-Core
    GIT_TAG main
)

FetchContent_MakeAvailable(SupercellCore)

# Polygon Packing
set(RP_ENABLE_DOWNLOADING ON)
set(LIBNEST2D_HEADER_ONLY ON)
FetchContent_Declare(
    libnest2d
    GIT_REPOSITORY https://github.com/tamasmeszaros/libnest2d.git
    GIT_TAG master
)
FetchContent_MakeAvailable(libnest2d)

# Image Processing
set(OPENCV_FORCE_3RDPARTY_BUILD ON)
set(WITH_OPENMP ON)
set(WITH_NVCUVENC OFF)
set(WITH_NVCUVID OFF)
set(WITH_FFMPEG OFF)
set(WITH_JASPER OFF)
set(WITH_OPENJPEG OFF)
set(WITH_JPEG OFF)
set(WITH_WEBP OFF)
set(WITH_TIFF OFF)
set(WITH_V4L OFF)
set(WITH_DSHOW OFF)
set(WITH_MSMF OFF)
set(WITH_DIRECTX OFF)
set(WITH_DIRECTML OFF)
set(WITH_OPENCL_D3D11_NV OFF)
set(WITH_PROTOBUF OFF)
set(WITH_IMGCODEC_PXM OFF)
set(WITH_IMGCODEC_PFM OFF)
set(WITH_QUIRC OFF)
set(BUILD_opencv_apps OFF)
set(BUILD_opencv_js OFF)
set(BUILD_ANDROID_PROJECTS OFF)
set(BUILD_ANDROID_EXAMPLES OFF)
set(BUILD_DOCS OFF)
set(BUILD_PACKAGE OFF)
set(BUILD_JAVA OFF)

if (NOT DEFINED BUILD_LIST)
    set(BUILD_LIST 
        core
        imgproc
        imgcodecs
    )
endif()

FetchContent_Declare(
        opencv
        GIT_REPOSITORY https://github.com/opencv/opencv.git
        GIT_TAG 4.6.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(opencv)
