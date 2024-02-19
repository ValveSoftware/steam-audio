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

get_host_bin_subdir(IPL_BIN_SUBDIR)

find_path(FlatBuffers_INCLUDE_DIR
	NAMES 			flatbuffers/flatbuffers.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/flatbuffers/include
)

find_program(FlatBuffers_EXECUTABLE
	NAMES	flatc
	PATHS 	${CMAKE_HOME_DIRECTORY}/deps/flatbuffers/bin/${IPL_BIN_SUBDIR}
)

if (FlatBuffers_INCLUDE_DIR)
	file(STRINGS ${FlatBuffers_INCLUDE_DIR}/flatbuffers/base.h _FlatBuffers_VERSION_MAJOR REGEX "^#define[\t ]+FLATBUFFERS_VERSION_MAJOR[\t ]+[0-9]+.*")
	file(STRINGS ${FlatBuffers_INCLUDE_DIR}/flatbuffers/base.h _FlatBuffers_VERSION_MINOR REGEX "^#define[\t ]+FLATBUFFERS_VERSION_MINOR[\t ]+[0-9]+.*")
	file(STRINGS ${FlatBuffers_INCLUDE_DIR}/flatbuffers/base.h _FlatBuffers_VERSION_PATCH REGEX "^#define[\t ]+FLATBUFFERS_VERSION_REVISION[\t ]+[0-9]+.*")
	string(REGEX REPLACE "^#define[\t ]+FLATBUFFERS_VERSION_MAJOR[\t ]+([0-9]+).*" "\\1" FlatBuffers_VERSION_MAJOR ${_FlatBuffers_VERSION_MAJOR})
	string(REGEX REPLACE "^#define[\t ]+FLATBUFFERS_VERSION_MINOR[\t ]+([0-9]+).*" "\\1" FlatBuffers_VERSION_MINOR ${_FlatBuffers_VERSION_MINOR})
	string(REGEX REPLACE "^#define[\t ]+FLATBUFFERS_VERSION_REVISION[\t ]+([0-9]+).*" "\\1" FlatBuffers_VERSION_PATCH ${_FlatBuffers_VERSION_PATCH})
	set(FlatBuffers_VERSION ${FlatBuffers_VERSION_MAJOR}.${FlatBuffers_VERSION_MINOR}.${FlatBuffers_VERSION_PATCH})
	set(FlatBuffers_VERSION_STRING ${FlatBuffers_VERSION})
endif()

find_package_handle_standard_args(FlatBuffers
	FOUND_VAR 		FlatBuffers_FOUND
	REQUIRED_VARS 	FlatBuffers_EXECUTABLE FlatBuffers_INCLUDE_DIR
	VERSION_VAR 	FlatBuffers_VERSION
)

if (FlatBuffers_FOUND)
	if (NOT TARGET FlatBuffers::FlatBuffers)
		add_library(FlatBuffers INTERFACE)
		target_include_directories(FlatBuffers INTERFACE ${FlatBuffers_INCLUDE_DIR})

		add_library(FlatBuffers::FlatBuffers ALIAS FlatBuffers)
	endif()
endif()

mark_as_advanced(FlatBuffers_INCLUDE_DIR FlatBuffers_EXECUTABLE)
