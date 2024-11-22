include(FetchContent)

set(CLIPPER2_UTILS OFF)
set(CLIPPER2_EXAMPLES OFF)
set(CLIPPER2_TESTS OFF)
set(CLIPPER2_USINGZ "OFF")
FetchContent_Declare(
    Clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2.git
    SOURCE_SUBDIR CPP
    GIT_TAG Clipper2_1.4.0
)
FetchContent_MakeAvailable(Clipper2)