# Copyright 2017-2023 Valve Corporation. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

add_custom_command(
    OUTPUT              ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
    DEPENDS             ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
                        ${CMAKE_CURRENT_SOURCE_DIR}/../src/steamaudio_fmod.h
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
						${CMAKE_CURRENT_SOURCE_DIR}/build-instructions.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reference.rst
						${CMAKE_CURRENT_SOURCE_DIR}/effects.rst
						${CMAKE_CURRENT_SOURCE_DIR}/spatializer.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reverb.rst
						${CMAKE_CURRENT_SOURCE_DIR}/mixer-return.rst
						${CMAKE_CURRENT_SOURCE_DIR}/api-reference.rst
                        ${CMAKE_CURRENT_BINARY_DIR}/xml/index.xml
	COMMAND 			${Sphinx_EXECUTABLE} -b html -Dbreathe_projects.SteamAudioFMOD=${CMAKE_CURRENT_BINARY_DIR}/xml -Dversion=${CMAKE_PROJECT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/site
	WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
)

add_custom_target(fmoddocs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/site/index.html)

install(
    DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}/site/
    DESTINATION doc
	PATTERN		".buildinfo" EXCLUDE
	PATTERN		".doctrees" EXCLUDE
)
