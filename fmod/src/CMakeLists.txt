#
# Copyright (c) Valve Corporation. All rights reserved.
#

set (SRC_FMOD
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
    add_bundle (phonon_fmod ${SRC_FMOD})
else ()
    add_library (phonon_fmod SHARED ${SRC_FMOD})
endif ()
