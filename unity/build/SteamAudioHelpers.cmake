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

function (get_local_lib_path LIB_PATH)
	if (IPL_OS_WINDOWS)
		if (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/windows-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/windows-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_LINUX)
		if (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/linux-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/linux-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_MACOS)
		set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/osx PARENT_SCOPE)
	elseif (IPL_OS_ANDROID)
		if (IPL_CPU_ARMV7)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-armv7 PARENT_SCOPE)
		elseif (IPL_CPU_ARMV8)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-armv8 PARENT_SCOPE)
		elseif (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_IOS)
		set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/ios PARENT_SCOPE)
	elseif (IPL_OS_WASM)
		set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/wasm PARENT_SCOPE)
	endif()
endfunction()

function (get_bin_subdir BIN_SUBDIR)
	if (IPL_OS_WINDOWS)
		if (IPL_CPU_X86)
			set(${BIN_SUBDIR} windows-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} windows-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_LINUX)
		if (IPL_CPU_X86)
			set(${BIN_SUBDIR} linux-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} linux-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_MACOS)
		set(${BIN_SUBDIR} osx PARENT_SCOPE)
	elseif (IPL_OS_ANDROID)
		if (IPL_CPU_ARMV7)
			set(${BIN_SUBDIR} android-armv7 PARENT_SCOPE)
		elseif (IPL_CPU_ARMV8)
			set(${BIN_SUBDIR} android-armv8 PARENT_SCOPE)
		elseif (IPL_CPU_X86)
			set(${BIN_SUBDIR} android-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} android-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_IOS)
		set(${BIN_SUBDIR} ios PARENT_SCOPE)
	elseif (IPL_OS_WASM)
		set(${BIN_SUBDIR} wasm PARENT_SCOPE)
	endif()
endfunction()
