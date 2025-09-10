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

get_host_bin_subdir(IPL_BIN_SUBDIR)

find_program(ISPC_EXECUTABLE
	NAMES	ispc
	PATHS 	${CMAKE_HOME_DIRECTORY}/deps/ispc/bin/${IPL_BIN_SUBDIR}
)

find_package_handle_standard_args(ISPC
	FOUND_VAR 		ISPC_FOUND
	REQUIRED_VARS 	ISPC_EXECUTABLE
	VERSION_VAR 	ISPC_VERSION
)

mark_as_advanced(ISPC_EXECUTABLE)
