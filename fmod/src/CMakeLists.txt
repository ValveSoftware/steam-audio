#
# Copyright (c) Valve Corporation. All rights reserved.
#

if (WIN32)
    set (SRC_FMOD_VERSION phonon_fmod.rc.in)
    set (SRC_FMOD_VERSION phonon_fmod.rc)
endif()

set (SRC_FMOD
    ${SRC_FMOD_VERSION}
    auto_load_library.h
    auto_load_library.cpp
    audio_engine_settings.h
    audio_engine_settings.cpp
    environment_proxy.h
    environment_proxy.cpp
    spatializer_effect.cpp
    mix_return_effect.cpp
    reverb_effect.cpp
    steamaudio_fmod.h
    steamaudio_fmod.cpp
)

if (APPLE)
    add_bundle(phonon_fmod_bundle ${SRC_FMOD})
    set_target_properties(phonon_fmod_bundle PROPERTIES OUTPUT_NAME "phonon_fmod")
endif()

add_library(phonon_fmod SHARED ${SRC_FMOD})

if (LINUX)
    target_link_libraries (phonon_fmod dl pthread)
endif ()
