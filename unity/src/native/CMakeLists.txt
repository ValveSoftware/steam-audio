#
# Copyright (c) Valve Corporation. All rights reserved.
#

set (SRC_UNITYNATIVE
    auto_load_library.h
    auto_load_library.cpp
	steamaudio_unity_native.h
    steamaudio_unity_native.cpp
    audio_engine_settings.h
    audio_engine_settings.cpp
    environment_proxy.h
    environment_proxy.cpp
    spatialize_effect.cpp
    reverb_effect.cpp
    mix_return_effect.cpp
)

if (APPLE)
    add_bundle (audioplugin_phonon ${SRC_UNITYNATIVE})
else ()
    add_library (audioplugin_phonon SHARED ${SRC_UNITYNATIVE})
endif ()    

if (MSVC)
    if (BUILDING_64BIT)
        add_custom_command (TARGET audioplugin_phonon POST_BUILD 
                            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/x86_64/)
    else ()
        add_custom_command (TARGET audioplugin_phonon POST_BUILD 
                            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/x86/)
    endif ()
elseif (APPLE)
    add_custom_command (TARGET audioplugin_phonon POST_BUILD
                        COMMAND cp -Rv ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}-$<LOWER_CASE:$<CONFIG>>/audioplugin_phonon.bundle ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/)
elseif (ANDROID)
    add_custom_command (TARGET audioplugin_phonon POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/android/)
else ()
    if (BUILDING_64BIT)
        add_custom_command (TARGET audioplugin_phonon POST_BUILD 
                            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/x86_64/)
    else ()
        add_custom_command (TARGET audioplugin_phonon POST_BUILD 
                            COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:audioplugin_phonon> ${CMAKE_HOME_DIRECTORY}/src/project/SteamAudioUnity/Assets/Plugins/x86/)
    endif ()
endif ()