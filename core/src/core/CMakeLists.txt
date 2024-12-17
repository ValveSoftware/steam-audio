# Copyright 2017-2023 Valve Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

include(SteamAudioHelpers)

#
# VERSION STAMPING
#

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon_version.h.in ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/phonon.rc.in ${CMAKE_CURRENT_BINARY_DIR}/phonon.rc)


#
# FLATBUFFERS
#

function (compile_fbs FBS_SOURCE)
    set(FBS_OUTPUT ${FBS_SOURCE}.h)

    set(FlatBuffers_FLAGS --cpp --scoped-enums --filename-suffix .fbs)

    add_custom_command(
        OUTPUT      ${CMAKE_CURRENT_BINARY_DIR}/${FBS_OUTPUT}
        DEPENDS     ${CMAKE_CURRENT_SOURCE_DIR}/${FBS_SOURCE}
        COMMAND     ${FlatBuffers_EXECUTABLE} ${FlatBuffers_FLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/ ${CMAKE_CURRENT_SOURCE_DIR}/${FBS_SOURCE}
        COMMENT     "Compiling FlatBuffers schema ${FBS_SOURCE}..."
        VERBATIM
    )
endfunction()

set(FBS_SOURCES
    vector.fbs
    sphere.fbs
    triangle.fbs
    mesh.fbs
    material.fbs
    static_mesh.fbs
    scene.fbs
    energy_field.fbs
    reverb.fbs
    baked_reflection_data.fbs
    path_visibility.fbs
    path_data.fbs
    probe_batch.fbs
)

foreach (FBS_FILE IN LISTS FBS_SOURCES)
    compile_fbs(${FBS_FILE})
    list(APPEND FBS_OUTPUTS ${CMAKE_CURRENT_BINARY_DIR}/${FBS_FILE}.h)
endforeach ()

add_custom_target(fbschemas DEPENDS ${FBS_OUTPUTS})
set_source_files_properties(${FBS_OUTPUTS} PROPERTIES GENERATED TRUE)


#
# ISPC
#

if (STEAMAUDIO_ENABLE_EMBREE)
    if (WIN32)
        set(ISPC_OBJECT_EXTENSION obj)
    else()
        set(ISPC_OBJECT_EXTENSION o)
    endif()

    set(ISPC_SOURCE         ${CMAKE_CURRENT_SOURCE_DIR}/embree_reflection_simulator.ispc)
    set(ISPC_HEADER         ${CMAKE_CURRENT_BINARY_DIR}/embree_reflection_simulator.ispc.h)
    set(ISPC_OBJECT         ${CMAKE_CURRENT_BINARY_DIR}/ispckernels.${ISPC_OBJECT_EXTENSION})
    set(ISPC_OBJECT_SSE2    ${CMAKE_CURRENT_BINARY_DIR}/ispckernels_sse2.${ISPC_OBJECT_EXTENSION})
    set(ISPC_OBJECT_SSE4    ${CMAKE_CURRENT_BINARY_DIR}/ispckernels_sse4.${ISPC_OBJECT_EXTENSION})
    set(ISPC_OBJECT_AVX     ${CMAKE_CURRENT_BINARY_DIR}/ispckernels_avx.${ISPC_OBJECT_EXTENSION})
    set(ISPC_OBJECT_AVX2    ${CMAKE_CURRENT_BINARY_DIR}/ispckernels_avx2.${ISPC_OBJECT_EXTENSION})
    set(ISPC_OUTPUTS        ${ISPC_HEADER} ${ISPC_OBJECT} ${ISPC_OBJECT_SSE2} ${ISPC_OBJECT_SSE4} ${ISPC_OBJECT_AVX} ${ISPC_OBJECT_AVX2})

    set(ISPC_FLAGS          -I ${CMAKE_HOME_DIRECTORY}/deps/embree/include -g)
    set(ISPC_FLAGS_HEADER   --target=avx2-i32x8)
    set(ISPC_FLAGS_OBJECT   --target=sse2-i32x4,sse4-i32x4,avx1-i32x8,avx2-i32x8)

    if (NOT IPL_OS_WINDOWS)
        list(APPEND ISPC_FLAGS --pic)
    endif()

    if (IPL_CPU_X86)
        list(APPEND ISPC_FLAGS --arch=x86)
    endif()

    add_custom_command(
        OUTPUT      ${ISPC_OUTPUTS}
        DEPENDS     ${ISPC_SOURCE}
        COMMAND     ${ISPC_EXECUTABLE} ${ISPC_FLAGS} ${ISPC_FLAGS_HEADER} -h ${ISPC_HEADER} ${ISPC_SOURCE}
        COMMAND     ${ISPC_EXECUTABLE} ${ISPC_FLAGS} ${ISPC_FLAGS_OBJECT} -o ${ISPC_OBJECT} ${ISPC_SOURCE}
        VERBATIM
    )

    # Ideally, we just want to add a manual dependency such that ispckernels gets built
    # before core, and the ISPC object files are passed as "source" files to any target that
    # links against core, which should be possible via target_sources(PUBLIC) on core and
    # target_link_libraries(). However, CMake doesn't propagate the GENERATED property on
    # source files correctly, so this results in errors when the build system is generated.
    # As a workaround, we create static library out of the ISPC object files and link it to
    # everything that links to core via target_link_libraries(PUBLIC) on core.
    #
    # https://gitlab.kitware.com/cmake/cmake/-/issues/18399
    add_library(ispckernels STATIC ${ISPC_OUTPUTS} pch.cpp)
    set_source_files_properties(${ISPC_OUTPUTS} PROPERTIES GENERATED TRUE)
    set_source_files_properties(${ISPC_OBJECT} ${ISPC_OBJECT_SSE2} ${ISPC_OBJECT_SSE4} ${ISPC_OBJECT_AVX} ${ISPC_OBJECT_AVX2} PROPERTIES EXTERNAL_OBJECT TRUE)
    set_target_properties(ispckernels PROPERTIES LINKER_LANGUAGE CXX)
endif()


#
# OPENCL
#

if (STEAMAUDIO_ENABLE_RADEONRAYS)
    set(CL_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/radeonrays_reflection_simulator.cl)
    set(CL_HEADER ${CMAKE_CURRENT_BINARY_DIR}/radeonrays_reflection_simulator.cl.h)

    add_custom_command (
        OUTPUT      ${CL_HEADER}
        DEPENDS     ${CL_SOURCE}
        COMMAND     ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/EmbedKernels.py ${CMAKE_CURRENT_SOURCE_DIR} ${CL_HEADER}
        VERBATIM
    )

    add_custom_target(clkernels ALL DEPENDS ${CL_HEADER})
endif()


#
# hrtf2sh
#

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    add_executable(hrtf2sh
        log.cpp
        memory_allocator.cpp
        context.cpp
        math_functions.cpp
        ipp_array_math.cpp
        ipp_fft.cpp
        sh/spherical_harmonics.cc
        sh.cpp
        audio_buffer.cpp
        hrtf_map.cpp
        hrtf_map_factory.cpp
        hrtf_database.cpp
        speaker_layout.cpp
        panning_effect.cpp
        ambisonics_panning_effect.cpp
        hrtf2sh.cpp
    )

    set_target_properties(hrtf2sh PROPERTIES EXCLUDE_FROM_ALL TRUE)
    target_include_directories(hrtf2sh PRIVATE ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(hrtf2sh PRIVATE IPL_DISABLE_SOFA)
    target_link_libraries(hrtf2sh PRIVATE ${FFT_LIBRARY})
endif()


#
# HRTF DATA
#

add_library(hrtf OBJECT hrtf.cpp)
target_include_directories(hrtf PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})


#
# CORE OBJECT LIBRARY
#

add_library(core OBJECT
    platform.h
    types.h
    error.h
    log.h
    log.cpp
    memory_allocator.h
    memory_allocator.cpp
    profiler.h
    profiler.cpp
    context.h
    context.cpp
    library.h
    library.cpp

    array.h
    containers.h
    stack.h
    triple_buffer.h
    serialized_object.h
    serialized_object.cpp

    float4.h
    sse_float4.h
    neon_float4.h
	array_math.h

    math_functions.h
    math_functions.cpp
    vector.h
    vector.fbs
    polar_vector.h
    matrix.h
    quaternion.h
    coordinate_space.h
    box.h
    sphere.h
    sphere.fbs
    sampling.h
    sampling.cpp

    window_function.h
    window_function.cpp
    iir.h
    iir.cpp
	fft.h
	bands.h
	bands.cpp

    sh/spherical_harmonics.h
    sh/spherical_harmonics.cc
    sh.h
    sh.cpp

    speaker_layout.h
    speaker_layout.cpp
    audio_buffer.h
    audio_buffer.cpp

    panning_effect.h
    panning_effect.cpp
    hrtf_map.h
    hrtf_map.cpp
    sofa_hrtf_map.h
    sofa_hrtf_map.cpp
    hrtf_database.h
    hrtf_database.cpp
    overlap_add_convolution_effect.h
    overlap_add_convolution_effect.cpp
    binaural_effect.h
    binaural_effect.cpp
    virtual_surround_effect.h
    virtual_surround_effect.cpp
    ambisonics_encode_effect.h
    ambisonics_encode_effect.cpp
    ambisonics_panning_effect.h
    ambisonics_panning_effect.cpp
    ambisonics_binaural_effect.h
    ambisonics_binaural_effect.cpp
    ambisonics_rotate_effect.h
    ambisonics_rotate_effect.cpp
    ambisonics_decode_effect.h
    ambisonics_decode_effect.cpp

    triangle.h
    triangle.fbs
    mesh.h
    mesh.cpp
    mesh.fbs
    ray.h
    ray.cpp
    hit.h
    bvh.h
    bvh.cpp
    material.h
    material.fbs

    propagation_medium.h
    static_mesh.h
    static_mesh.cpp
    static_mesh.fbs
    instanced_mesh.h
    instanced_mesh.cpp
    scene.h
    scene.cpp
    scene.fbs
    custom_scene.h
    custom_scene.cpp

	distance_attenuation.h
	distance_attenuation.cpp
	air_absorption.h
	air_absorption.cpp
    directivity.h
    directivity.cpp
    direct_simulator.h
    direct_simulator.cpp

    gain_effect.h
    gain_effect.cpp
    eq_effect.h
    eq_effect.cpp
    delay_effect.h
    delay_effect.cpp
    direct_effect.h
    direct_effect.cpp

    job.h
    job_graph.h
    job_graph.cpp
    thread_pool.h
    thread_pool.cpp

    energy_field.h
    energy_field.cpp
    energy_field.fbs
    reflection_simulator.h
    reflection_simulator.cpp

    impulse_response.h
    impulse_response.cpp
    reconstructor.h
    reconstructor.cpp

    reverb_estimator.h
    reverb_estimator.cpp
    reverb.fbs

    hybrid_reverb_estimator.h
    hybrid_reverb_estimator.cpp

    probe.h
    probe_generator.h
    probe_generator.cpp
    probe_tree.h
    probe_tree.cpp
    probe_data.h
    probe_batch.h
    probe_batch.cpp
    probe_batch.fbs
    probe_manager.h
    probe_manager.cpp

    baked_reflection_data.h
    baked_reflection_data.cpp
    baked_reflection_data.fbs
    reflection_baker.h
    reflection_baker.cpp
    baked_reflection_simulator.h
    baked_reflection_simulator.cpp

    overlap_save_convolution_effect.h
    overlap_save_convolution_effect.cpp
 	delay.h
	delay.cpp
	reverb_effect.h
	reverb_effect.cpp
    hybrid_reverb_effect.h
    hybrid_reverb_effect.cpp
    indirect_effect.h
    indirect_effect.cpp

    path_visibility.h
    path_visibility.cpp
    path_visibility.fbs
    path_finder.h
    path_finder.cpp
    path_data.h
    path_data.cpp
    path_data.fbs
    path_simulator.h
    path_simulator.cpp
    path_effect.h
    path_effect.cpp

    simulation_data.h
    simulation_data.cpp
    simulation_manager.h
    simulation_manager.cpp

	hrtf_map_factory.h
	hrtf_map_factory.cpp
    scene_factory.h
    scene_factory.cpp
    energy_field_factory.h
    energy_field_factory.cpp
    reflection_simulator_factory.h
    reflection_simulator_factory.cpp
    impulse_response_factory.h
    impulse_response_factory.cpp
    reconstructor_factory.h
    reconstructor_factory.cpp

	phonon_version.h.in
    phonon.h
    docs.h
    util.h
    phonon_interfaces.h
    api_context.h
    api_context.cpp
    api_geometry.cpp
    api_serialized_object.h
    api_serialized_object.cpp
    api_embree_device.h
    api_embree_device.cpp
    api_opencl_device.h
    api_opencl_device.cpp
    api_radeonrays_device.h
    api_radeonrays_device.cpp
    api_tan_device.h
    api_tan_device.cpp
    api_scene.h
    api_scene.cpp
    api_audio_buffer.cpp
    api_hrtf.h
    api_hrtf.cpp
    api_panning_effect.h
    api_panning_effect.cpp
    api_binaural_effect.h
    api_binaural_effect.cpp
    api_virtual_surround_effect.h
    api_virtual_surround_effect.cpp
    api_ambisonics_encode_effect.h
    api_ambisonics_encode_effect.cpp
    api_ambisonics_panning_effect.h
    api_ambisonics_panning_effect.cpp
    api_ambisonics_binaural_effect.h
    api_ambisonics_binaural_effect.cpp
    api_ambisonics_rotate_effect.h
    api_ambisonics_rotate_effect.cpp
    api_ambisonics_decode_effect.h
    api_ambisonics_decode_effect.cpp
    api_direct_effect.h
    api_direct_effect.cpp
    api_indirect_effect.h
    api_indirect_effect.cpp
    api_path_effect.h
    api_path_effect.cpp
    api_probes.h
    api_probes.cpp
    api_baking.cpp
    api_simulator.h
    api_simulator.cpp
    api_advanced_simulation.cpp
    api_validation_layer.cpp
)

if (STEAMAUDIO_PROFILING_TOOL STREQUAL "None")
    target_sources(core PRIVATE
        null_profiler.h
        null_profiler.cpp
    )
endif()

if (IPL_OS_WINDOWS)
    target_sources(core PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/phonon.rc
    )
endif()

if (STEAMAUDIO_ENABLE_IPP AND (IPL_OS_WINDOWS OR IPL_OS_LINUX OR IPL_OS_MACOS))
    target_sources(core PRIVATE
        ipp_array_math.cpp
    )
    if (IPL_OS_MACOS)
        target_sources(core PRIVATE
            array_math.cpp
        )
    endif()
else()
    target_sources(core PRIVATE
        array_math.cpp
    )
endif()

if (FFT_LIBRARY STREQUAL "IPP")
    target_sources(core PRIVATE
        ipp_fft.cpp
    )
    if (IPL_OS_MACOS)
        target_sources(core PRIVATE
            vdsp_fft.cpp
        )
    endif()
elseif (FFT_LIBRARY STREQUAL "FFTS")
    target_sources(core PRIVATE
        ffts_fft.cpp
    )
elseif (FFT_LIBRARY STREQUAL "PFFFT")
    target_sources(core PRIVATE
        pffft_fft.cpp
    )
endif()

if (STEAMAUDIO_ENABLE_AVX)
    target_sources(core PRIVATE
        float8.h
        avx_float8.h
        float8_iir.cpp
        float8_delay.cpp
        float8_reverb_effect.cpp
    )
	if (IPL_OS_WINDOWS)
        set_source_files_properties(
            float8_iir.cpp
            float8_delay.cpp
            float8_reverb_effect.cpp
            PROPERTIES
                COMPILE_FLAGS "/arch:AVX"
        )
	endif()
endif()

if (STEAMAUDIO_ENABLE_EMBREE)
    target_sources(core PRIVATE
        embree_device.h
        embree_device.cpp
        embree_static_mesh.h
        embree_static_mesh.cpp
        embree_instanced_mesh.h
        embree_instanced_mesh.cpp
        embree_scene.h
        embree_scene.cpp
        embree_reflection_simulator.h
        embree_reflection_simulator.cpp
        embree_reflection_simulator.ispc
    )
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS OR STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    target_sources(core PRIVATE
        opencl_device.h
        opencl_device.cpp
        opencl_buffer.h
        opencl_buffer.cpp
        opencl_kernel.h
        opencl_kernel.cpp
    )
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS)
    target_sources(core PRIVATE
        radeonrays_device.h
        radeonrays_device.cpp
        radeonrays_static_mesh.h
        radeonrays_static_mesh.cpp
        radeonrays_scene.h
        radeonrays_scene.cpp
        opencl_energy_field.h
        opencl_energy_field.cpp
        radeonrays_reflection_simulator.h
        radeonrays_reflection_simulator.cpp
        radeonrays_reflection_simulator.cl
        opencl_impulse_response.h
        opencl_impulse_response.cpp
        opencl_reconstructor.h
        opencl_reconstructor.cpp
    )
endif()

if (STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    target_sources(core PRIVATE
        tan_device.h
        tan_device.cpp
        tan_convolution_effect.h
        tan_convolution_effect.cpp
    )
endif()

if (MSVC)
    target_sources(core PRIVATE phonon.natvis)
endif()

if (STEAMAUDIO_BUILD_CSHARP_BINDINGS)
    target_sources(core PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/phonon_csharp_wrap.cxx)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/phonon_csharp_wrap.cxx PROPERTIES GENERATED TRUE)
endif()

# This is needed so we can include generated headers
target_include_directories(core PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

if (IPL_OS_WINDOWS)
    target_link_libraries(core PUBLIC delayimp)
elseif (IPL_OS_LINUX)
    target_link_libraries(core PUBLIC m dl pthread)
elseif (IPL_OS_ANDROID)
    target_link_libraries(core PUBLIC log android)
endif()

if (IPL_OS_LINUX)
    target_link_libraries(core PUBLIC -Wl,--start-group)
endif()

target_link_libraries(core PUBLIC FlatBuffers::FlatBuffers)

if (STEAMAUDIO_ENABLE_IPP AND (IPL_OS_WINDOWS OR IPL_OS_LINUX OR IPL_OS_MACOS))
    target_link_libraries(core PUBLIC IPP::IPP)
endif()

if (FFT_LIBRARY STREQUAL "IPP")
    target_link_libraries(core PUBLIC IPP::IPP)
    if (IPL_OS_MACOS)
        target_link_libraries(core PUBLIC "-framework Accelerate")
    endif()
elseif (FFT_LIBRARY STREQUAL "FFTS")
    target_link_libraries(core PUBLIC FFTS::FFTS)
elseif (FFT_LIBRARY STREQUAL "PFFFT")
    target_link_libraries(core PUBLIC PFFFT::PFFFT)
endif()

target_link_libraries(core PUBLIC MySOFA::MySOFA)
add_dependencies(core fbschemas)

if (STEAMAUDIO_ENABLE_EMBREE)
    target_link_libraries(core PUBLIC Embree::Embree ispckernels)
endif()

if (STEAMAUDIO_ENABLE_RADEONRAYS)
    target_link_libraries(core PUBLIC RadeonRays::RadeonRays)
    add_dependencies(core clkernels)
endif()

if (STEAMAUDIO_ENABLE_TRUEAUDIONEXT)
    target_link_libraries(core PUBLIC TrueAudioNext::TrueAudioNext)
endif()

if (IPL_OS_LINUX)
    target_link_libraries(core PUBLIC -Wl,--end-group)
endif()

source_group("FlatBuffers Schema Files" FILES ${FBS_SOURCES})
source_group("ISPC Files" FILES ${ISPC_SOURCE})
source_group("OpenCL Files" FILES ${CL_SOURCE})

target_compile_definitions(core PRIVATE STEAMAUDIO_BUILDING_CORE)

target_precompile_headers(core PUBLIC pch.h)


#
# STATIC OR DYNAMIC LIBRARY
#

if (BUILD_SHARED_LIBS AND NOT IPL_OS_IOS)
    add_library(phonon SHARED pch.cpp)
else()
    add_library(phonon STATIC pch.cpp)
endif()

target_link_libraries(phonon PRIVATE core hrtf)

if (IPL_OS_MACOS)
    add_library(phonon_bundle MODULE pch.cpp)
    set_target_properties(phonon_bundle PROPERTIES BUNDLE TRUE)
    set_target_properties(phonon_bundle PROPERTIES OUTPUT_NAME phonon)
    target_link_libraries(phonon_bundle PRIVATE core hrtf)
endif()

target_compile_definitions(phonon PRIVATE STEAMAUDIO_BUILDING_CORE)
if (IPL_OS_MACOS)
    target_compile_definitions(phonon PRIVATE STEAMAUDIO_BUILDING_CORE)
endif()

if (IPL_OS_LINUX)
    target_link_options(phonon PRIVATE -Wl,--exclude-libs,ALL)
endif()

if (IPL_OS_WINDOWS AND IPL_CPU_X64 AND BUILD_SHARED_LIBS)
    set_target_properties(phonon PROPERTIES LINK_FLAGS "/DELAYLOAD:opencl.dll /DELAYLOAD:gpuutilities.dll /DELAYLOAD:trueaudionext.dll")
endif()

if (IPL_OS_LINUX AND BUILD_SHARED_LIBS AND (NOT IPL_CPU_ARMV8))
    add_custom_command(
        TARGET      phonon
        POST_BUILD
        COMMAND     objcopy --only-keep-debug $<TARGET_FILE_NAME:phonon> $<TARGET_FILE_NAME:phonon>.dbg
        COMMAND     strip --strip-debug --strip-unneeded $<TARGET_FILE_NAME:phonon>
        COMMAND     objcopy --add-gnu-debuglink=$<TARGET_FILE_NAME:phonon>.dbg $<TARGET_FILE_NAME:phonon>
        COMMENT     "Extracting debug symbols..."
    )
endif()


#
# POST-BUILD STEP FOR COPYING core -> fmod
#

set(FMOD_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/include/phonon)

set(FMOD_LIB_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/windows-x86)
    elseif (IPL_CPU_X64)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/windows-x64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/linux-x86)
    elseif (IPL_CPU_X64)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/linux-x64)
    endif()
elseif (IPL_OS_MACOS)
    set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/osx)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/android-armv7)
    elseif (IPL_CPU_ARMV8)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/android-armv8)
    elseif (IPL_CPU_X86)
        set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/android-x86)
    endif()
elseif (IPL_OS_IOS)
    set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/ios)
elseif (IPL_OS_WASM)
    set(FMOD_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../fmod/lib/wasm)
endif()

if (NOT FMOD_LIB_DIR STREQUAL "")
    if (IPL_OS_IOS)
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${FMOD_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:PFFFT::PFFFT> ${FMOD_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MySOFA::MySOFA> ${FMOD_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${FMOD_LIB_DIR}
        )
    else()
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${FMOD_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${FMOD_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${FMOD_LIB_DIR}
        )
    endif()

    if (IPL_OS_MACOS)
        # CMake's Xcode generator runs post-build steps before code signing, so the copied files don't have the signature applied.
        # This causes issues in newer versions of macOS, which refuse to load .bundles that don't have a signature.
        add_custom_target(phonon_bundle_copy_fmod ALL
            COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:phonon_bundle> ${FMOD_LIB_DIR}/phonon.bundle
            VERBATIM
        )
        add_dependencies(phonon_bundle_copy_fmod phonon_bundle)
    endif()
endif()


#
# POST-BUILD STEP FOR COPYING core -> wwise
#

set(WWISE_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/../wwise/include/phonon)

if (NOT WWISE_INCLUDE_DIR STREQUAL "")
    add_custom_command(
        TARGET      phonon
        POST_BUILD
        COMMAND     ${CMAKE_COMMAND} -E make_directory ${WWISE_INCLUDE_DIR}
        COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${WWISE_INCLUDE_DIR}
        COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${WWISE_INCLUDE_DIR}
        COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${WWISE_INCLUDE_DIR}
    )
endif()


#
# POST-BUILD STEP FOR COPYING core -> unity
#

set(UNITY_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/../unity/include/phonon)

set(UNITY_LIB_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/windows-x86)
    elseif (IPL_CPU_X64)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/windows-x64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/linux-x86)
    elseif (IPL_CPU_X64)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/linux-x64)
    endif()
elseif (IPL_OS_MACOS)
    set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/osx)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/android-armv7)
    elseif (IPL_CPU_ARMV8)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/android-armv8)
    elseif (IPL_CPU_X86)
        set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/android-x86)
    endif()
elseif (IPL_OS_IOS)
    set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/ios)
elseif (IPL_OS_WASM)
    set(UNITY_LIB_DIR ${CMAKE_HOME_DIRECTORY}/../unity/lib/wasm)
endif()

set(UNITY_PLUGIN_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Windows/x86_64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X86)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86)
    elseif (IPL_CPU_X64)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Linux/x86_64)
    endif()
elseif (IPL_OS_MACOS)
    set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/macOS)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/armv7)
    elseif (IPL_CPU_ARMV8)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/arm64)
    elseif (IPL_CPU_X86)
        set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/Android/x86)
    endif()
elseif (IPL_OS_IOS)
    set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/iOS)
elseif (IPL_OS_WASM)
    set(UNITY_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unity/src/project/SteamAudioUnity/Assets/Plugins/SteamAudio/Binaries/HTML5)
endif()

if (NOT UNITY_PLUGIN_DIR STREQUAL "")
    if (IPL_OS_MACOS)
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNITY_LIB_DIR}
        )

        # CMake's Xcode generator runs post-build steps before code signing, so the copied files don't have the signature applied.
        # This causes issues in newer versions of macOS, which refuse to load .bundles that don't have a signature.
        add_custom_target(phonon_bundle_copy_unity ALL
            COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:phonon_bundle> ${UNITY_LIB_DIR}/phonon.bundle
            COMMAND     ${CMAKE_COMMAND} -E copy_directory $<TARGET_BUNDLE_DIR:phonon_bundle> ${UNITY_PLUGIN_DIR}/phonon.bundle
            VERBATIM
        )
        add_dependencies(phonon_bundle_copy_unity phonon_bundle)
    elseif (IPL_OS_IOS)
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNITY_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:PFFFT::PFFFT> ${UNITY_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MySOFA::MySOFA> ${UNITY_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNITY_PLUGIN_DIR}
        )
    else()
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNITY_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNITY_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNITY_LIB_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNITY_PLUGIN_DIR}
        )
    endif()
endif()


#
# POST-BUILD STEP FOR COPYING core -> unreal
#

set(UNREAL_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/include)

set(UNREAL_PLUGIN_DIR "")
if (IPL_OS_WINDOWS)
    if (IPL_CPU_X86)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x86)
    elseif (IPL_CPU_X64)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/windows-x64)
    endif()
elseif (IPL_OS_LINUX)
    if (IPL_CPU_X64)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/linux-x64)
    endif()
elseif (IPL_OS_MACOS)
    set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/osx)
elseif (IPL_OS_ANDROID)
    if (IPL_CPU_ARMV7)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/armeabi-v7a)
    elseif (IPL_CPU_ARMV8)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/arm64-v8a)
    elseif (IPL_CPU_X86)
        set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/android/x86)
    endif()
elseif (IPL_OS_IOS)
    set(UNREAL_PLUGIN_DIR ${CMAKE_HOME_DIRECTORY}/../unreal/src/SteamAudioUnreal/Plugins/SteamAudio/Source/SteamAudioSDK/lib/ios)
endif()

if (NOT UNREAL_PLUGIN_DIR STREQUAL "")
    if (IPL_OS_IOS)
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:PFFFT::PFFFT> ${UNREAL_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:MySOFA::MySOFA> ${UNREAL_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNREAL_PLUGIN_DIR}
        )
    elseif (IPL_OS_MACOS)
        # CMake's Xcode generator runs post-build steps before code signing, so the copied files don't have the signature applied.
        # This causes issues in newer versions of macOS, which refuse to load .bundles that don't have a signature.
        add_custom_target(phonon_copy_unreal ALL
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNREAL_PLUGIN_DIR}
            VERBATIM
        )
        add_dependencies(phonon_copy_unreal phonon)
    else()
        add_custom_command(
            TARGET      phonon
            POST_BUILD
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E make_directory ${UNREAL_PLUGIN_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h ${UNREAL_INCLUDE_DIR}
            COMMAND     ${CMAKE_COMMAND} -E copy $<TARGET_FILE:phonon> ${UNREAL_PLUGIN_DIR}
        )
    endif()
endif()


#
# INSTALL
#

get_bin_subdir(IPL_BIN_SUBDIR)

install(
    TARGETS             phonon
    ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
    LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
    RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
)

if (IPL_OS_MACOS)
    install(
        TARGETS             phonon_bundle
        ARCHIVE DESTINATION lib/${IPL_BIN_SUBDIR}
        LIBRARY DESTINATION lib/${IPL_BIN_SUBDIR}
        RUNTIME DESTINATION lib/${IPL_BIN_SUBDIR}
    )
endif()

if (IPL_OS_WINDOWS)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/phonon.pdb
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
elseif (IPL_OS_LINUX AND NOT IPL_CPU_ARMV8)
    install(
        FILES       ${CMAKE_CURRENT_BINARY_DIR}/libphonon.so.dbg
        DESTINATION symbols/${IPL_BIN_SUBDIR}
    )
endif()

install(
    FILES       ${CMAKE_CURRENT_SOURCE_DIR}/phonon.h
                ${CMAKE_CURRENT_BINARY_DIR}/phonon_version.h
                ${CMAKE_CURRENT_SOURCE_DIR}/phonon_interfaces.h
    DESTINATION include
)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    install(
        FILES       ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/bin/windows-x64/$<LOWER_CASE:$<CONFIG>>/TrueAudioNext.dll
                    ${CMAKE_HOME_DIRECTORY}/deps/trueaudionext/bin/windows-x64/$<LOWER_CASE:$<CONFIG>>/GPUUtilities.dll
        DESTINATION lib/${IPL_BIN_SUBDIR}
    )
endif()

include(CMakeListsInternal.txt OPTIONAL)
