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

find_path(DRWAV_INCLUDE_DIR
	NAMES 			dr_wav.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/drwav/include
)

find_package_handle_standard_args(DRWAV
	FOUND_VAR 		DRWAV_FOUND
	REQUIRED_VARS 	DRWAV_INCLUDE_DIR
)

if (DRWAV_FOUND)
	if (NOT TARGET DRWAV::DRWAV)
		add_library(DRWAV INTERFACE)
		target_include_directories(DRWAV INTERFACE ${DRWAV_INCLUDE_DIR})
		target_sources(DRWAV INTERFACE
			${DRWAV_INCLUDE_DIR}/dr_wav.h
		)

		add_library(DRWAV::DRWAV ALIAS DRWAV)
	endif()
endif()

mark_as_advanced(DRWAV_INCLUDE_DIR)
