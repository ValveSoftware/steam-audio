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

# ---------------------------------------------------------------------------------------------------------------------
# DEPENDENCIES
# ---------------------------------------------------------------------------------------------------------------------

include(SteamAudioHelpers)

get_host_bin_subdir(IPL_BIN_SUBDIR)

set(Doxygen_ROOT ${CMAKE_HOME_DIRECTORY}/../core/deps/doxygen/bin/${IPL_BIN_SUBDIR})
set(DOXYGEN_SKIP_DOT TRUE)
find_package(Doxygen)

find_package(Sphinx)

if (NOT DOXYGEN_FOUND OR NOT Sphinx_FOUND)
    message(STATUS "Disabling docs")
    set(STEAMAUDIOWWISE_BUILD_DOCS OFF)
endif()

if (NOT STEAMAUDIOWWISE_BUILD_DOCS)
    return()
endif()


# ---------------------------------------------------------------------------------------------------------------------
# API REFERENCE
# ---------------------------------------------------------------------------------------------------------------------

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
)

add_custom_command(
    OUTPUT              ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
    DEPENDS             ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                        ${CMAKE_CURRENT_SOURCE_DIR}/../src/SoundEnginePlugin/SteamAudioCommon.h
    COMMAND             ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)


# ---------------------------------------------------------------------------------------------------------------------
# MAIN DOCUMENTATION
# ---------------------------------------------------------------------------------------------------------------------

add_custom_command(
    OUTPUT              ${CMAKE_CURRENT_BINARY_DIR}/site/index.html
    DEPENDS             ${CMAKE_CURRENT_SOURCE_DIR}/conf.py
                        ${CMAKE_CURRENT_SOURCE_DIR}/index.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/getting-started.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/guide.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/build-instructions.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/reference.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/effects.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/spatializer.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/reverb.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/mixer-return.rst
                        ${CMAKE_CURRENT_SOURCE_DIR}/api-reference.rst
                        ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
    COMMAND             ${Sphinx_EXECUTABLE} -b html -Dbreathe_projects.SteamAudioWwise=${CMAKE_CURRENT_BINARY_DIR}/xml -Dversion=${CMAKE_PROJECT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/site
    WORKING_DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}
    VERBATIM
)


# ---------------------------------------------------------------------------------------------------------------------
# BUILD TARGET
# ---------------------------------------------------------------------------------------------------------------------

add_custom_target(wwisedocs ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/site/index.html
)


# ---------------------------------------------------------------------------------------------------------------------
# INSTALL
# ---------------------------------------------------------------------------------------------------------------------

install(
    DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}/site/
    DESTINATION doc
    PATTERN     ".buildinfo"    EXCLUDE
    PATTERN     ".doctrees"     EXCLUDE
)
