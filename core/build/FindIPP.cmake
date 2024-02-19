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

find_path(IPP_INCLUDE_DIR
	NAMES 			ipp.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/ipp/include
)

find_library(IPP_core_LIBRARY
	NAMES ippcoremt ippcore
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/ipp/lib/${IPL_BIN_SUBDIR}
)

find_library(IPP_vm_LIBRARY
	NAMES ippvmmt ippvm
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/ipp/lib/${IPL_BIN_SUBDIR}
)

find_library(IPP_s_LIBRARY
	NAMES ippsmt ipps
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/ipp/lib/${IPL_BIN_SUBDIR}
)

if (IPP_INCLUDE_DIR)
	file(STRINGS ${IPP_INCLUDE_DIR}/ippversion.h _IPP_VERSION_MAJOR REGEX "^#define[\t ]+IPP_VERSION_MAJOR[\t ]+.*")
	file(STRINGS ${IPP_INCLUDE_DIR}/ippversion.h _IPP_VERSION_MINOR REGEX "^#define[\t ]+IPP_VERSION_MINOR[\t ]+.*")
	file(STRINGS ${IPP_INCLUDE_DIR}/ippversion.h _IPP_VERSION_PATCH REGEX "^#define[\t ]+IPP_VERSION_UPDATE[\t ]+.*")
	string(REGEX REPLACE "^#define[\t ]+IPP_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1" IPP_VERSION_MAJOR ${_IPP_VERSION_MAJOR})
	string(REGEX REPLACE "^#define[\t ]+IPP_VERSION_MINOR[\t ]+([0-9]+).*" "\\1" IPP_VERSION_MINOR ${_IPP_VERSION_MINOR})
	string(REGEX REPLACE "^#define[\t ]+IPP_VERSION_UPDATE[\t ]+([0-9]+).*" "\\1" IPP_VERSION_PATCH ${_IPP_VERSION_PATCH})
	set(IPP_VERSION ${IPP_VERSION_MAJOR}.${IPP_VERSION_MINOR}.${IPP_VERSION_PATCH})
	set(IPP_VERSION_STRING ${IPP_VERSION})
endif()

find_package_handle_standard_args(IPP
	FOUND_VAR 		IPP_FOUND
	REQUIRED_VARS 	IPP_core_LIBRARY IPP_vm_LIBRARY IPP_s_LIBRARY IPP_INCLUDE_DIR
	VERSION_VAR 	IPP_VERSION
)

if (IPP_FOUND)
	if (NOT TARGET IPP::IPP)
		add_library(IPP::core UNKNOWN IMPORTED)
		set_target_properties(IPP::core PROPERTIES IMPORTED_LOCATION ${IPP_core_LIBRARY})

		add_library(IPP::vm UNKNOWN IMPORTED)
		set_target_properties(IPP::vm PROPERTIES IMPORTED_LOCATION ${IPP_vm_LIBRARY})

		add_library(IPP::s UNKNOWN IMPORTED)
		set_target_properties(IPP::s PROPERTIES IMPORTED_LOCATION ${IPP_s_LIBRARY})

		add_library(IPP INTERFACE)
		target_include_directories(IPP INTERFACE ${IPP_INCLUDE_DIR})
		target_link_libraries(IPP INTERFACE IPP::core IPP::vm IPP::s)

		add_library(IPP::IPP ALIAS IPP)
	endif()
endif()

mark_as_advanced(IPP_INCLUDE_DIR IPP_core_LIBRARY IPP_vm_LIBRARY IPP_s_LIBRARY)
