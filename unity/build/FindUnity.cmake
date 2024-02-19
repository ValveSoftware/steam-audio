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

include(FindPackageHandleStandardArgs)

find_path(Unity_INCLUDE_DIR
	NAMES 			AudioPluginInterface.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/include
	PATH_SUFFIXES 	unity5
)

find_program(Unity_EXECUTABLE
	NAMES Unity.exe
	PATHS ${Unity_EXECUTABLE_DIR}
)

find_package_handle_standard_args(Unity
	FOUND_VAR 		Unity_FOUND
	REQUIRED_VARS 	Unity_INCLUDE_DIR
)

if (Unity_FOUND)
	if (NOT TARGET Unity::NativeAudio)
		add_library(UnityNativeAudio INTERFACE)
		target_include_directories(UnityNativeAudio INTERFACE ${Unity_INCLUDE_DIR})

		add_library(Unity::NativeAudio ALIAS UnityNativeAudio)
	endif()
endif()

mark_as_advanced(Unity_INCLUDE_DIR)
