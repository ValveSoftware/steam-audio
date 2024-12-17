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
# INTERACTIVE TESTS
#

add_executable(phonon_itest
	itest.h
	itest.cpp
	ui_window.h
	ui_window.cpp
	ui_audio_engine.h
	ui_audio_engine.cpp
	helpers.h
	helpers.cpp
	itest_gui.cpp
	itest_audioengine.cpp
	itest_windowfunction.cpp
	itest_iir.cpp
	itest_bvh.cpp
	itest_raytracer.cpp
	itest_instancedmesh.cpp
	itest_energyfield.cpp
	itest_impulseresponse.cpp
	itest_parametricreverb.cpp
	itest_probes.cpp
	itest_panningeffect.cpp
	itest_binauraleffect.cpp
	itest_virtualsurroundeffect.cpp
	itest_ambisonicspanningeffect.cpp
	itest_ambisonicsbinauraleffect.cpp
	itest_ambisonicsrotateeffect.cpp
	itest_gaineffect.cpp
	itest_eqeffect.cpp
	itest_delayeffect.cpp
	itest_directsoundeffect.cpp
	itest_reverbeffect.cpp
	itest_directsimulator.cpp
	itest_convolutioneffect.cpp
	itest_hybridreverbeffect.cpp
	itest_bakedindirect.cpp
	itest_pathing.cpp
	itest_energycurves.cpp
)

target_link_libraries(phonon_itest PRIVATE core hrtf GLFW::GLFW IMGUI::IMGUI IMPLOT::IMPLOT PortAudio::PortAudio DRWAV::DRWAV)

target_compile_definitions(phonon_itest PRIVATE STEAMAUDIO_BUILDING_CORE)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    set_target_properties(phonon_itest PROPERTIES LINK_FLAGS "/DELAYLOAD:opencl.dll /DELAYLOAD:gpuutilities.dll /DELAYLOAD:trueaudionext.dll")
endif()

# FIXME: Some build issues with GLFW when PCHs are used
target_precompile_headers(phonon_itest PRIVATE pch.h)

get_bin_subdir(IPL_BIN_SUBDIR)
install(TARGETS phonon_itest RUNTIME DESTINATION bin/${IPL_BIN_SUBDIR})

include(CMakeListsInternal.txt OPTIONAL)
