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

find_package(OpenGL)

if (OPENGL_FOUND)
	find_path(GLFW_INCLUDE_DIR
		NAMES			GLFW/glfw3.h
		PATHS			${CMAKE_HOME_DIRECTORY}/deps/glfw/include
	)

	find_library(GLFW_LIBRARY
		NAMES glfw3
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/glfw/lib/${IPL_BIN_SUBDIR}/release
	)

	find_library(GLFW_LIBRARY_DEBUG
		NAMES glfw3
		PATHS ${CMAKE_HOME_DIRECTORY}/deps/glfw/lib/${IPL_BIN_SUBDIR}/debug
	)

	if (GLFW_INCLUDE_DIR)
		file(STRINGS ${GLFW_INCLUDE_DIR}/GLFW/glfw3.h _GLFW_VERSION_MAJOR REGEX "^#define[\t ]+GLFW_VERSION_MAJOR[\t ]+.*")
		file(STRINGS ${GLFW_INCLUDE_DIR}/GLFW/glfw3.h _GLFW_VERSION_MINOR REGEX "^#define[\t ]+GLFW_VERSION_MINOR[\t ]+.*")
		file(STRINGS ${GLFW_INCLUDE_DIR}/GLFW/glfw3.h _GLFW_VERSION_PATCH REGEX "^#define[\t ]+GLFW_VERSION_REVISION[\t ]+.*")
		string(REGEX REPLACE "^#define[\t ]+GLFW_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1" GLFW_VERSION_MAJOR ${_GLFW_VERSION_MAJOR})
		string(REGEX REPLACE "^#define[\t ]+GLFW_VERSION_MINOR[\t ]+([0-9]+).*" "\\1" GLFW_VERSION_MINOR ${_GLFW_VERSION_MINOR})
		string(REGEX REPLACE "^#define[\t ]+GLFW_VERSION_REVISION[\t ]+([0-9]+).*" "\\1" GLFW_VERSION_PATCH ${_GLFW_VERSION_PATCH})
		set(GLFW_VERSION ${GLFW_VERSION_MAJOR}.${GLFW_VERSION_MINOR}.${GLFW_VERSION_PATCH})
		set(GLFW_VERSION_STRING ${GLFW_VERSION})
	endif()

	find_package_handle_standard_args(GLFW
		FOUND_VAR 		GLFW_FOUND
		REQUIRED_VARS 	GLFW_LIBRARY GLFW_INCLUDE_DIR
		VERSION_VAR 	GLFW_VERSION
	)

	if (GLFW_FOUND)
		if (NOT TARGET GLFW::GLFW)
			add_library(GLFW::GLFW UNKNOWN IMPORTED)
			set_target_properties(GLFW::GLFW PROPERTIES IMPORTED_LOCATION ${GLFW_LIBRARY})
			if (GLFW_LIBRARY_DEBUG)
				set_target_properties(GLFW::GLFW PROPERTIES IMPORTED_LOCATION_DEBUG ${GLFW_LIBRARY_DEBUG})
			endif()
			target_include_directories(GLFW::GLFW INTERFACE ${GLFW_INCLUDE_DIR})
			target_link_libraries(GLFW::GLFW INTERFACE OpenGL::GL OpenGL::GLU)
		endif()
	endif()

	mark_as_advanced(GLFW_INCLUDE_DIR GLFW_LIBRARY)
endif()
