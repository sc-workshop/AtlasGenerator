include(FetchContent)

set(RP_ENABLE_DOWNLOADING ON)
FetchContent_Declare(
    libnest2d
    GIT_REPOSITORY https://github.com/Daniil-SV/libnest2d.git
    GIT_TAG dev
)
FetchContent_MakeAvailable(libnest2d)