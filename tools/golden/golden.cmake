# Golden data generator target. Not a standalone project: included into the reference build by hook.cmake,
# in the scope of core/CMakeLists.txt, so source paths are made absolute.

set(GOLDEN_DIR ${CMAKE_CURRENT_LIST_DIR})

add_executable(golden
    ${GOLDEN_DIR}/src/main.cpp
    ${GOLDEN_DIR}/src/fft.cpp
    ${GOLDEN_DIR}/src/geometry.cpp
    ${GOLDEN_DIR}/src/arrays.cpp
    ${GOLDEN_DIR}/src/audio_buffer.cpp
    ${GOLDEN_DIR}/src/sh.cpp
    ${GOLDEN_DIR}/src/dsp.cpp
    ${GOLDEN_DIR}/src/hrtf.cpp
    ${GOLDEN_DIR}/src/convolution.cpp
    ${GOLDEN_DIR}/src/spatialization.cpp
    ${GOLDEN_DIR}/src/ambisonics.cpp
    ${GOLDEN_DIR}/src/sofa.cpp
)

target_link_libraries(golden PRIVATE core hrtf)
target_compile_definitions(golden PRIVATE STEAMAUDIO_BUILDING_CORE)
target_include_directories(golden PRIVATE ${GOLDEN_DIR}/src)

# Upstream builds as C++14; the generator only needs C++17 for std::filesystem.
set_target_properties(golden PROPERTIES CXX_STANDARD 17)
