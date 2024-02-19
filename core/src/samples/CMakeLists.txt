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
# SAMPLES
#

add_executable(sample_binaural
    sample_binaural.cpp
)

target_link_libraries(sample_binaural PRIVATE core hrtf)

target_compile_definitions(sample_binaural PRIVATE STEAMAUDIO_BUILDING_CORE)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    set_target_properties(sample_binaural PROPERTIES LINK_FLAGS "/DELAYLOAD:opencl.dll /DELAYLOAD:gpuutilities.dll /DELAYLOAD:trueaudionext.dll")
endif()

if (IPL_OS_IOS)
    set_target_properties(sample_binaural PROPERTIES EXCLUDE_FROM_ALL TRUE)
    set_target_properties(sample_binaural PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.valve.sample_binaural)
    set_target_properties(sample_binaural PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION 1.0.0)
    set_target_properties(sample_binaural PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0.0)
endif()

if (NOT IPL_OS_IOS)
    get_bin_subdir(IPL_BIN_SUBDIR)
    install(TARGETS sample_binaural RUNTIME DESTINATION bin/${IPL_BIN_SUBDIR})
endif()
