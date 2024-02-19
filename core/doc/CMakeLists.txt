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

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_command(
    OUTPUT              ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
    DEPENDS             ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/phonon.h
                        ${CMAKE_CURRENT_SOURCE_DIR}/../src/core/docs.h
    COMMAND             ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)

add_custom_command(
	OUTPUT				${CMAKE_CURRENT_BINARY_DIR}/site/index.html
	DEPENDS 			${CMAKE_CURRENT_SOURCE_DIR}/conf.py
						${CMAKE_CURRENT_SOURCE_DIR}/index.rst
						${CMAKE_CURRENT_SOURCE_DIR}/getting-started.rst
						${CMAKE_CURRENT_SOURCE_DIR}/guide.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/integration.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reference.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/types.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/context.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/geometry.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/serialization.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/embree.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/opencl.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/radeon-rays.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/trueaudio-next.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/scene.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/audio-buffers.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/hrtf.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/panning-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/binaural-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/virtual-surround-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/ambisonics-encode-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/ambisonics-panning-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/ambisonics-binaural-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/ambisonics-rotation-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/ambisonics-decode-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/direct-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/reflections-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/path-effect.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/probes.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/baking.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/simulation.rst
                        ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
	COMMAND 			${Sphinx_EXECUTABLE} -b html -Dbreathe_projects.SteamAudio=${CMAKE_CURRENT_BINARY_DIR}/xml -Dversion=${CMAKE_PROJECT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/site
	WORKING_DIRECTORY	${CMAKE_CURRENT_BINARY_DIR}
	VERBATIM
)

add_custom_target(apidocs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/site/index.html)

install(
    DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}/site/
    DESTINATION doc
	PATTERN		".buildinfo" EXCLUDE
	PATTERN		".doctrees" EXCLUDE
)
