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

find_path(IMPLOT_INCLUDE_DIR
	NAMES 			implot.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/implot/include
)

find_package_handle_standard_args(IMPLOT
	FOUND_VAR 		IMPLOT_FOUND
	REQUIRED_VARS 	IMPLOT_INCLUDE_DIR
)

if (IMPLOT_FOUND)
	if (NOT TARGET IMPLOT::IMPLOT)
		add_library(IMPLOT INTERFACE)
		target_include_directories(IMPLOT INTERFACE ${IMPLOT_INCLUDE_DIR})
		target_sources(IMPLOT INTERFACE
			${IMPLOT_INCLUDE_DIR}/implot.h
			${IMPLOT_INCLUDE_DIR}/implot_internal.h
			${IMPLOT_INCLUDE_DIR}/implot.cpp
			${IMPLOT_INCLUDE_DIR}/implot_items.cpp
		)

		add_library(IMPLOT::IMPLOT ALIAS IMPLOT)
	endif()
endif()

mark_as_advanced(IMPLOT_INCLUDE_DIR)
