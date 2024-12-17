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

set(Sphinx_EXECUTABLE_DIR "" CACHE PATH "Directory containing the Sphinx binary.")

find_package(Python3)

if (Python3_FOUND)
	find_program(Sphinx_EXECUTABLE
		NAMES	sphinx-build
		PATHS 	${Sphinx_EXECUTABLE_DIR}
	)
endif()

find_package_handle_standard_args(Sphinx
	FOUND_VAR 		Sphinx_FOUND
	REQUIRED_VARS 	Sphinx_EXECUTABLE
)

mark_as_advanced(Sphinx_EXECUTABLE_DIR Sphinx_EXECUTABLE)
