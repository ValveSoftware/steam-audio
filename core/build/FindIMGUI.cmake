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

find_path(IMGUI_INCLUDE_DIR
	NAMES 			imgui.h
	PATHS 			${CMAKE_HOME_DIRECTORY}/deps/imgui/include
)

find_package_handle_standard_args(IMGUI
	FOUND_VAR 		IMGUI_FOUND
	REQUIRED_VARS 	IMGUI_INCLUDE_DIR
)

if (IMGUI_FOUND)
	if (NOT TARGET IMGUI::IMGUI)
		add_library(IMGUI INTERFACE)
		target_include_directories(IMGUI INTERFACE ${IMGUI_INCLUDE_DIR})
		target_sources(IMGUI INTERFACE
			${IMGUI_INCLUDE_DIR}/imconfig.h
			${IMGUI_INCLUDE_DIR}/imgui.h
			${IMGUI_INCLUDE_DIR}/imgui_internal.h
			${IMGUI_INCLUDE_DIR}/imstb_rectpack.h
			${IMGUI_INCLUDE_DIR}/imstb_textedit.h
			${IMGUI_INCLUDE_DIR}/imstb_truetype.h
			${IMGUI_INCLUDE_DIR}/imgui_impl_glfw.h
			${IMGUI_INCLUDE_DIR}/imgui_impl_opengl2.h
			${IMGUI_INCLUDE_DIR}/imgui.cpp
			${IMGUI_INCLUDE_DIR}/imgui_demo.cpp
			${IMGUI_INCLUDE_DIR}/imgui_draw.cpp
			${IMGUI_INCLUDE_DIR}/imgui_tables.cpp
			${IMGUI_INCLUDE_DIR}/imgui_widgets.cpp
			${IMGUI_INCLUDE_DIR}/imgui_impl_glfw.cpp
			${IMGUI_INCLUDE_DIR}/imgui_impl_opengl2.cpp
		)

		add_library(IMGUI::IMGUI ALIAS IMGUI)
	endif()
endif()

mark_as_advanced(IMGUI_INCLUDE_DIR)
