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

# todo: deps? direct link to core?

include(FindPackageHandleStandardArgs)

find_path(SteamAudio_INCLUDE_DIR
    NAMES 			phonon.h
    PATHS 			${CMAKE_HOME_DIRECTORY}/include
    PATH_SUFFIXES 	phonon
)

find_package_handle_standard_args(SteamAudio
    FOUND_VAR 		SteamAudio_FOUND
    REQUIRED_VARS 	SteamAudio_INCLUDE_DIR
)

if (SteamAudio_FOUND AND NOT TARGET SteamAudio::SteamAudio)
    add_library(SteamAudio::SteamAudio UNKNOWN IMPORTED)
    target_include_directories(SteamAudio::SteamAudio INTERFACE ${SteamAudio_INCLUDE_DIR})
endif()

mark_as_advanced(SteamAudio_INCLUDE_DIR)
