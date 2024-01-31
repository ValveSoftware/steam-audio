# Copyright 2017-2023 Valve Corporation. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

add_custom_command(
	OUTPUT				${CMAKE_CURRENT_BINARY_DIR}/site/index.html
	DEPENDS 			${CMAKE_CURRENT_SOURCE_DIR}/conf.py
						${CMAKE_CURRENT_SOURCE_DIR}/index.rst
						${CMAKE_CURRENT_SOURCE_DIR}/getting-started.rst
						${CMAKE_CURRENT_SOURCE_DIR}/guide.rst
						${CMAKE_CURRENT_SOURCE_DIR}/build-instructions.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reference.rst
						${CMAKE_CURRENT_SOURCE_DIR}/components.rst
						${CMAKE_CURRENT_SOURCE_DIR}/ambisonic-source.rst
						${CMAKE_CURRENT_SOURCE_DIR}/baked-listener.rst
						${CMAKE_CURRENT_SOURCE_DIR}/baked-source.rst
						${CMAKE_CURRENT_SOURCE_DIR}/dynamic-object.rst
						${CMAKE_CURRENT_SOURCE_DIR}/geometry.rst
						${CMAKE_CURRENT_SOURCE_DIR}/listener.rst
						${CMAKE_CURRENT_SOURCE_DIR}/probe-batch.rst
						${CMAKE_CURRENT_SOURCE_DIR}/source.rst
						${CMAKE_CURRENT_SOURCE_DIR}/static-mesh.rst
						${CMAKE_CURRENT_SOURCE_DIR}/mixer-effects.rst
						${CMAKE_CURRENT_SOURCE_DIR}/reverb.rst
						${CMAKE_CURRENT_SOURCE_DIR}/mixer-return.rst
						${CMAKE_CURRENT_SOURCE_DIR}/asset-types.rst
						${CMAKE_CURRENT_SOURCE_DIR}/material.rst
						${CMAKE_CURRENT_SOURCE_DIR}/settings.rst
						${CMAKE_CURRENT_SOURCE_DIR}/menu-commands.rst
	COMMAND 			${Sphinx_EXECUTABLE} -b html -Dversion=${CMAKE_PROJECT_VERSION} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/site
	WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
	VERBATIM
)

add_custom_target(unitydocs ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/site/index.html)

install(
    DIRECTORY   ${CMAKE_CURRENT_BINARY_DIR}/site/
    DESTINATION doc
	PATTERN		".buildinfo" EXCLUDE
	PATTERN		".doctrees" EXCLUDE
)
