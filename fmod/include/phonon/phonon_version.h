/*
 *  Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
 *  https://valvesoftware.github.io/steam-audio/license.html
 */

#ifndef IPL_PHONON_VERSION_H
#define IPL_PHONON_VERSION_H

#define STEAMAUDIO_VERSION_MAJOR 4
#define STEAMAUDIO_VERSION_MINOR 0
#define STEAMAUDIO_VERSION_PATCH 0
#define STEAMAUDIO_VERSION       (((uint32_t)(STEAMAUDIO_VERSION_MAJOR) << 16) | \
                                  ((uint32_t)(STEAMAUDIO_VERSION_MINOR) << 8) |  \
                                  ((uint32_t)(STEAMAUDIO_VERSION_PATCH)))

#endif
