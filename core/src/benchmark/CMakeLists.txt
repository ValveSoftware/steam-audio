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
# PERFORMANCE BENCHMARKING
#

add_executable(phonon_perf
	phonon_perf.h
	phonon_perf.cpp
	benchmark_panning.cpp
	benchmark_binaural.cpp
	benchmark_binauralrenderer.cpp
	benchmark_virtualsurround.cpp
	benchmark_ambisonicsbinaural.cpp
	benchmark_ambisonicsrotation.cpp
	benchmark_reverb.cpp
	benchmark_scene.cpp
	benchmark_reflections.cpp
	benchmark_raytracer.cpp
	benchmark_reconstruction.cpp
	benchmark_convolution.cpp
	benchmark_directsoundeffect.cpp
	benchmark_baking.cpp
	benchmark_binauraleffect.cpp
	benchmark_patheffect.cpp
	benchmark_pathingbake.cpp
	benchmark_pathing.cpp
	benchmark_probelookup.cpp
)

target_link_libraries(phonon_perf PRIVATE core hrtf)

target_compile_definitions(phonon_perf PRIVATE STEAMAUDIO_BUILDING_CORE)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
	set_target_properties(phonon_perf PROPERTIES LINK_FLAGS "/DELAYLOAD:opencl.dll /DELAYLOAD:gpuutilities.dll /DELAYLOAD:trueaudionext.dll")
endif()

target_precompile_headers(phonon_perf PRIVATE pch.h)

if (IPL_OS_IOS)
	set_target_properties(phonon_perf PROPERTIES EXCLUDE_FROM_ALL TRUE)
	set_target_properties(phonon_perf PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.valve.phonon_perf)
	set_target_properties(phonon_perf PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION 1.0.0)
	set_target_properties(phonon_perf PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0.0)
endif()

if (NOT IPL_OS_IOS)
	get_bin_subdir(IPL_BIN_SUBDIR)
	install(TARGETS phonon_perf RUNTIME DESTINATION bin/${IPL_BIN_SUBDIR})
endif()
