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
include(SteamAudioHelpers)

get_bin_subdir(IPL_BIN_SUBDIR)

find_path(Embree_INCLUDE_DIR
	NAMES			rtcore.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/embree/include
)

find_library(Embree_lexers_LIBRARY
	NAMES lexers
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_math_LIBRARY
	NAMES math
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_simd_LIBRARY
	NAMES simd
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_sys_LIBRARY
	NAMES sys
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_tasking_LIBRARY
	NAMES tasking
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_sse2_LIBRARY
	NAMES embree
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_sse4_LIBRARY
	NAMES embree_sse42
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_avx_LIBRARY
	NAMES embree_avx
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_avx2_LIBRARY
	NAMES embree_avx2
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/release
)

find_library(Embree_lexers_LIBRARY_DEBUG
	NAMES lexers
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_math_LIBRARY_DEBUG
	NAMES math
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_simd_LIBRARY_DEBUG
	NAMES simd
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_sys_LIBRARY_DEBUG
	NAMES sys
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_tasking_LIBRARY_DEBUG
	NAMES tasking
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_sse2_LIBRARY_DEBUG
	NAMES embree
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_sse4_LIBRARY_DEBUG
	NAMES embree_sse42
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_avx_LIBRARY_DEBUG
	NAMES embree_avx
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

find_library(Embree_avx2_LIBRARY_DEBUG
	NAMES embree_avx2
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/embree/lib/${IPL_BIN_SUBDIR}/debug
)

if (Embree_INCLUDE_DIR)
	file(STRINGS ${Embree_INCLUDE_DIR}/rtcore_version.h _Embree_VERSION_MAJOR REGEX "^#define[\t ]+RTCORE_VERSION_MAJOR[\t ]+.*")
	file(STRINGS ${Embree_INCLUDE_DIR}/rtcore_version.h _Embree_VERSION_MINOR REGEX "^#define[\t ]+RTCORE_VERSION_MINOR[\t ]+.*")
	file(STRINGS ${Embree_INCLUDE_DIR}/rtcore_version.h _Embree_VERSION_PATCH REGEX "^#define[\t ]+RTCORE_VERSION_PATCH[\t ]+.*")
	string(REGEX REPLACE "^#define[\t ]+RTCORE_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1" Embree_VERSION_MAJOR ${_Embree_VERSION_MAJOR})
	string(REGEX REPLACE "^#define[\t ]+RTCORE_VERSION_MINOR[\t ]+([0-9]+).*" "\\1" Embree_VERSION_MINOR ${_Embree_VERSION_MINOR})
	string(REGEX REPLACE "^#define[\t ]+RTCORE_VERSION_PATCH[\t ]+([0-9]+).*" "\\1" Embree_VERSION_PATCH ${_Embree_VERSION_PATCH})
	set(Embree_VERSION ${Embree_VERSION_MAJOR}.${Embree_VERSION_MINOR}.${Embree_VERSION_PATCH})
	set(Embree_VERSION_STRING ${Embree_VERSION})
endif()

find_package_handle_standard_args(Embree
	FOUND_VAR 		Embree_FOUND
	REQUIRED_VARS 	Embree_sse2_LIBRARY Embree_sse4_LIBRARY Embree_avx_LIBRARY Embree_avx2_LIBRARY
					Embree_lexers_LIBRARY Embree_math_LIBRARY Embree_simd_LIBRARY Embree_sys_LIBRARY Embree_tasking_LIBRARY
					Embree_INCLUDE_DIR
	VERSION_VAR 	Embree_VERSION
)

if (Embree_FOUND)
	if (NOT TARGET Embree::Embree)
		add_library(Embree::lexers UNKNOWN IMPORTED)
		set_target_properties(Embree::lexers PROPERTIES IMPORTED_LOCATION ${Embree_lexers_LIBRARY})
		if (Embree_lexers_LIBRARY_DEBUG)
			set_target_properties(Embree::lexers PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_lexers_LIBRARY_DEBUG})
		endif()

		add_library(Embree::math UNKNOWN IMPORTED)
		set_target_properties(Embree::math PROPERTIES IMPORTED_LOCATION ${Embree_math_LIBRARY})
		if (Embree_math_LIBRARY_DEBUG)
			set_target_properties(Embree::math PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_math_LIBRARY_DEBUG})
		endif()

		add_library(Embree::simd UNKNOWN IMPORTED)
		set_target_properties(Embree::simd PROPERTIES IMPORTED_LOCATION ${Embree_simd_LIBRARY})
		if (Embree_simd_LIBRARY_DEBUG)
			set_target_properties(Embree::simd PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_simd_LIBRARY_DEBUG})
		endif()

		add_library(Embree::sys UNKNOWN IMPORTED)
		set_target_properties(Embree::sys PROPERTIES IMPORTED_LOCATION ${Embree_sys_LIBRARY})
		if (Embree_sys_LIBRARY_DEBUG)
			set_target_properties(Embree::sys PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_sys_LIBRARY_DEBUG})
		endif()

		add_library(Embree::tasking UNKNOWN IMPORTED)
		set_target_properties(Embree::tasking PROPERTIES IMPORTED_LOCATION ${Embree_tasking_LIBRARY})
		if (Embree_tasking_LIBRARY_DEBUG)
			set_target_properties(Embree::tasking PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_tasking_LIBRARY_DEBUG})
		endif()

		add_library(Embree::sse2 UNKNOWN IMPORTED)
		set_target_properties(Embree::sse2 PROPERTIES IMPORTED_LOCATION ${Embree_sse2_LIBRARY})
		if (Embree_sse2_LIBRARY_DEBUG)
			set_target_properties(Embree::sse2 PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_sse2_LIBRARY_DEBUG})
		endif()

		add_library(Embree::sse4 UNKNOWN IMPORTED)
		set_target_properties(Embree::sse4 PROPERTIES IMPORTED_LOCATION ${Embree_sse4_LIBRARY})
		if (Embree_sse4_LIBRARY_DEBUG)
			set_target_properties(Embree::sse4 PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_sse4_LIBRARY_DEBUG})
		endif()

		add_library(Embree::avx UNKNOWN IMPORTED)
		set_target_properties(Embree::avx PROPERTIES IMPORTED_LOCATION ${Embree_avx_LIBRARY})
		if (Embree_avx_LIBRARY_DEBUG)
			set_target_properties(Embree::avx PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_avx_LIBRARY_DEBUG})
		endif()

		add_library(Embree::avx2 UNKNOWN IMPORTED)
		set_target_properties(Embree::avx2 PROPERTIES IMPORTED_LOCATION ${Embree_avx2_LIBRARY})
		if (Embree_avx2_LIBRARY_DEBUG)
			set_target_properties(Embree::avx2 PROPERTIES IMPORTED_LOCATION_DEBUG ${Embree_avx2_LIBRARY_DEBUG})
		endif()

		add_library(Embree INTERFACE)
		target_include_directories(Embree INTERFACE ${Embree_INCLUDE_DIR})
		target_link_libraries(Embree INTERFACE Embree::lexers Embree::math Embree::simd Embree::sys Embree::tasking Embree::sse2 Embree::sse4 Embree::avx Embree::avx2)

		add_library(Embree::Embree ALIAS Embree)
	endif()
endif()

mark_as_advanced(
	Embree_INCLUDE_DIR
	Embree_lexers_LIBRARY Embree_math_LIBRARY Embree_simd_LIBRARY Embree_sys_LIBRARY Embree_tasking_LIBRARY
	Embree_sse2_LIBRARY Embree_sse4_LIBRARY Embree_avx_LIBRARY Embree_avx2_LIBRARY
)
