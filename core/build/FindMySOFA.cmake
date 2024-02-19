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

set(ZLIB_ROOT ${CMAKE_HOME_DIRECTORY}/deps/zlib/lib/${IPL_BIN_SUBDIR}/release) # look here first
set(ZLIB_INCLUDE_DIR ${CMAKE_HOME_DIRECTORY}/deps/zlib/include) # deps/zlib is structured differently than a typical zlib install root, so point to this explicitly
find_package(ZLIB REQUIRED)

find_path(MySOFA_INCLUDE_DIR
	NAMES 			mysofa.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/mysofa/include
)

find_library(MySOFA_LIBRARY
	NAMES mysofa
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/mysofa/lib/${IPL_BIN_SUBDIR}/release
)

find_library(MySOFA_LIBRARY_DEBUG
	NAMES mysofa
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/mysofa/lib/${IPL_BIN_SUBDIR}/debug
)

find_package_handle_standard_args(MySOFA
	FOUND_VAR 		MySOFA_FOUND
	REQUIRED_VARS 	MySOFA_LIBRARY MySOFA_INCLUDE_DIR
)

if (MySOFA_FOUND)
	if (NOT TARGET MySOFA::MySOFA)
		add_library(MySOFA::MySOFA UNKNOWN IMPORTED)
		set_target_properties(MySOFA::MySOFA PROPERTIES IMPORTED_LOCATION ${MySOFA_LIBRARY})
		if (MySOFA_LIBRARY_DEBUG)
			set_target_properties(MySOFA::MySOFA PROPERTIES IMPORTED_LOCATION_DEBUG ${MySOFA_LIBRARY_DEBUG})
		endif()
		target_include_directories(MySOFA::MySOFA INTERFACE ${MySOFA_INCLUDE_DIR})
		target_link_libraries(MySOFA::MySOFA INTERFACE ZLIB::ZLIB)
	endif()
endif()

mark_as_advanced(MySOFA_INCLUDE_DIR MySOFA_LIBRARY)
