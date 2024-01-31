# Copyright 2017-2023 Valve Corporation. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

include(FindPackageHandleStandardArgs)

find_program(Sphinx_EXECUTABLE
	NAMES	sphinx-build
	PATHS 	${Sphinx_EXECUTABLE_DIR}
)

find_package_handle_standard_args(Sphinx
	FOUND_VAR 		Sphinx_FOUND
	REQUIRED_VARS 	Sphinx_EXECUTABLE
)

mark_as_advanced(Sphinx_EXECUTABLE)
