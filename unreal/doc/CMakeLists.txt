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

add_custom_command(
	OUTPUT				${CMAKE_CURRENT_BINARY_DIR}/site/index.html
	DEPENDS 			${CMAKE_CURRENT_SOURCE_DIR}/conf.py
						${CMAKE_CURRENT_SOURCE_DIR}/index.rst
						${CMAKE_CURRENT_SOURCE_DIR}/getting-started.rst
						${CMAKE_CURRENT_SOURCE_DIR}/guide.rst
						${CMAKE_CURRENT_SOURCE_DIR}/build-instructions.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reference.rst
						${CMAKE_CURRENT_SOURCE_DIR}/components.rst
						${CMAKE_CURRENT_SOURCE_DIR}/baked-listener.rst
						${CMAKE_CURRENT_SOURCE_DIR}/baked-source.rst
						${CMAKE_CURRENT_SOURCE_DIR}/dynamic-object.rst
						${CMAKE_CURRENT_SOURCE_DIR}/geometry.rst
						${CMAKE_CURRENT_SOURCE_DIR}/listener.rst
						${CMAKE_CURRENT_SOURCE_DIR}/source.rst
						${CMAKE_CURRENT_SOURCE_DIR}/actors.rst
						${CMAKE_CURRENT_SOURCE_DIR}/probe-volume.rst
						${CMAKE_CURRENT_SOURCE_DIR}/static-mesh.rst
						${CMAKE_CURRENT_SOURCE_DIR}/submix-effects.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reverb.rst
						${CMAKE_CURRENT_SOURCE_DIR}/asset-types.rst
						${CMAKE_CURRENT_SOURCE_DIR}/material.rst
						${CMAKE_CURRENT_SOURCE_DIR}/spatialization-settings.rst
						${CMAKE_CURRENT_SOURCE_DIR}/occlusion-settings.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reverb-settings.rst
						${CMAKE_CURRENT_SOURCE_DIR}/menu-commands.rst
						${CMAKE_CURRENT_SOURCE_DIR}/settings.rst
	COMMAND 			${Sphinx_EXECUTABLE} -b html -Dversion=${CMAKE_PROJECT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/site
	WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
)

add_custom_target(unrealdocs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/site/index.html)

install(
    DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}/site/
    DESTINATION doc
	PATTERN		".buildinfo" EXCLUDE
	PATTERN		".doctrees" EXCLUDE
)
