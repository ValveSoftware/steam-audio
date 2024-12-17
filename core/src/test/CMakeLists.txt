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
# UNIT TESTS
#

add_executable(phonon_test
	test.cpp
	Array.test.cpp
	AudioBuffer.test.cpp
	Bands.test.cpp
	Box.test.cpp
	BVH.test.cpp
	ConvolutionEffect.test.cpp
	CoordinateSpace.test.cpp
	DirectSimulator.test.cpp
	EnergyField.test.cpp
	Error.test.cpp
	FourierTransform.test.cpp
	Hit.test.cpp
	HRTFDatabase.test.cpp
	IirFilter.test.cpp
	ImpulseResponse.test.cpp
	Log.test.cpp
	Material.test.cpp
	MathFunctions.test.cpp
	Matrix.test.cpp
	Memory.test.cpp
	Mesh.test.cpp
	PolarVector.test.cpp
	ProbeTree.test.cpp
	Profiler.test.cpp
	Quaternion.test.cpp
	Ray.test.cpp
	ReflectionSimulator.test.cpp
	Sampling.test.cpp
	Scene.test.cpp
	Sphere.test.cpp
	SphericalHarmonics.test.cpp
	Stack.test.cpp
	StaticMesh.test.cpp
	Triangle.test.cpp
	Vector.test.cpp
	RayTracerCompare.test.cpp
	BinauralEffect.test.cpp
	AmbisonicsBinauralEffect.test.cpp
)

target_link_libraries(phonon_test PRIVATE core hrtf Catch::Catch)

target_compile_definitions(phonon_test PRIVATE STEAMAUDIO_BUILDING_CORE)

if (IPL_OS_WINDOWS AND IPL_CPU_X64)
    set_target_properties(phonon_test PROPERTIES LINK_FLAGS "/DELAYLOAD:opencl.dll /DELAYLOAD:gpuutilities.dll /DELAYLOAD:trueaudionext.dll")
endif()

if (IPL_OS_IOS)
	set_target_properties(phonon_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
	set_target_properties(phonon_test PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.valve.phonon_test)
	set_target_properties(phonon_test PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION 1.0.0)
	set_target_properties(phonon_test PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0.0)
endif()

target_precompile_headers(phonon_test PRIVATE pch.h)
set_property(SOURCE test.cpp PROPERTY SKIP_PRECOMPILE_HEADERS TRUE)

set_target_properties(phonon_test PROPERTIES UNITY_BUILD FALSE)

add_test(NAME UnitTests COMMAND phonon_test ~[ReflectionSimulator] WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if (NOT IPL_OS_IOS)
	get_bin_subdir(IPL_BIN_SUBDIR)
	install(TARGETS phonon_test RUNTIME DESTINATION bin/${IPL_BIN_SUBDIR})
endif()
