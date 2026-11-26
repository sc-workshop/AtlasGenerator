include(FetchContent)

set(CLIPPER2_UTILS OFF)
set(CLIPPER2_EXAMPLES OFF)
set(CLIPPER2_TESTS OFF)
set(CLIPPER2_USINGZ "OFF")
set(BUILD_SHARED_LIBS OFF)

if (NOT TARGET Clipper2)
    FetchContent_Declare(
        Clipper2
        GIT_REPOSITORY https://github.com/PolyClipping/Clipper2.git
        SOURCE_SUBDIR CPP
        GIT_TAG Clipper2_2.0.1
    )
    FetchContent_MakeAvailable(Clipper2)
endif()