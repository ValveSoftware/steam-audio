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

find_path(Catch_INCLUDE_DIR
	NAMES 			catch.hpp
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/catch/include
)

if (Catch_INCLUDE_DIR)
	file(STRINGS ${Catch_INCLUDE_DIR}/catch.hpp _Catch_VERSION REGEX "[\t ]+\\*[\t ]+Catch v[0-9]+\\.[0-9]+\\.[0-9]+.*")
	string(REGEX REPLACE "[\t ]+\\*[\t ]+Catch v([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\1" Catch_VERSION_MAJOR ${_Catch_VERSION})
	string(REGEX REPLACE "[\t ]+\\*[\t ]+Catch v([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\2" Catch_VERSION_MINOR ${_Catch_VERSION})
	string(REGEX REPLACE "[\t ]+\\*[\t ]+Catch v([0-9]+)\\.([0-9]+)\\.([0-9]+).*" "\\3" Catch_VERSION_PATCH ${_Catch_VERSION})
	set(Catch_VERSION ${Catch_VERSION_MAJOR}.${Catch_VERSION_MINOR}.${Catch_VERSION_PATCH})
	set(Catch_VERSION_STRING ${Catch_VERSION})
endif()

find_package_handle_standard_args(Catch
	FOUND_VAR 		Catch_FOUND
	REQUIRED_VARS 	Catch_INCLUDE_DIR
	VERSION_VAR 	Catch_VERSION
)

if (Catch_FOUND)
	if (NOT TARGET Catch::Catch)
		add_library(Catch INTERFACE)
		target_include_directories(Catch INTERFACE ${Catch_INCLUDE_DIR})

		add_library(Catch::Catch ALIAS Catch)
	endif()
endif()

mark_as_advanced(Catch_INCLUDE_DIR)
