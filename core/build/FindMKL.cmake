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

find_path(MKL_INCLUDE_DIR
	NAMES			mkl.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/mkl/include
)

find_library(MKL_core_LIBRARY
	NAMES mkl_core
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/mkl/lib/${IPL_BIN_SUBDIR}
)

find_library(MKL_sequential_LIBRARY
	NAMES mkl_sequential
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/mkl/lib/${IPL_BIN_SUBDIR}
)

find_library(MKL_intel_LIBRARY
	NAMES mkl_intel mkl_intel_c mkl_intel_lp64
	PATHS ${CMAKE_HOME_DIRECTORY}/deps/mkl/lib/${IPL_BIN_SUBDIR}
)

if (MKL_INCLUDE_DIR)
	file(STRINGS ${MKL_INCLUDE_DIR}/mkl_version.h _MKL_VERSION_MAJOR REGEX "^#define[\t ]+__INTEL_MKL__[\t ]+.*")
	file(STRINGS ${MKL_INCLUDE_DIR}/mkl_version.h _MKL_VERSION_MINOR REGEX "^#define[\t ]+__INTEL_MKL_MINOR__[\t ]+.*")
	file(STRINGS ${MKL_INCLUDE_DIR}/mkl_version.h _MKL_VERSION_PATCH REGEX "^#define[\t ]+__INTEL_MKL_UPDATE__[\t ]+.*")
	string(REGEX REPLACE "^#define[\t ]+__INTEL_MKL__[\t ]+([0-9]+).*" "\\1" MKL_VERSION_MAJOR ${_MKL_VERSION_MAJOR})
	string(REGEX REPLACE "^#define[\t ]+__INTEL_MKL_MINOR__[\t ]+([0-9]+).*" "\\1" MKL_VERSION_MINOR ${_MKL_VERSION_MINOR})
	string(REGEX REPLACE "^#define[\t ]+__INTEL_MKL_UPDATE__[\t ]+([0-9]+).*" "\\1" MKL_VERSION_PATCH ${_MKL_VERSION_PATCH})
	set(MKL_VERSION ${MKL_VERSION_MAJOR}.${MKL_VERSION_MINOR}.${MKL_VERSION_PATCH})
	set(MKL_VERSION_STRING ${MKL_VERSION})
endif()

find_package_handle_standard_args(MKL
	FOUND_VAR 		MKL_FOUND
	REQUIRED_VARS 	MKL_core_LIBRARY MKL_sequential_LIBRARY MKL_intel_LIBRARY MKL_INCLUDE_DIR
	VERSION_VAR 	MKL_VERSION
)

if (MKL_FOUND)
	if (NOT TARGET MKL::MKL)
		add_library(MKL::core UNKNOWN IMPORTED)
		set_target_properties(MKL::core PROPERTIES IMPORTED_LOCATION ${MKL_core_LIBRARY})

		add_library(MKL::sequential UNKNOWN IMPORTED)
		set_target_properties(MKL::sequential PROPERTIES IMPORTED_LOCATION ${MKL_sequential_LIBRARY})

		add_library(MKL::intel UNKNOWN IMPORTED)
		set_target_properties(MKL::intel PROPERTIES IMPORTED_LOCATION ${MKL_intel_LIBRARY})

		add_library(MKL INTERFACE)
		target_include_directories(MKL INTERFACE ${MKL_INCLUDE_DIR})
		target_link_libraries(MKL INTERFACE MKL::core MKL::sequential MKL::intel)

		add_library(MKL::MKL ALIAS MKL)
	endif()
endif()

mark_as_advanced(MKL_INCLUDE_DIR MKL_core_LIBRARY MKL_sequential_LIBRARY MKL_s_LIBRARY)
