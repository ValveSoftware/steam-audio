#
# Copyright (c) Valve Corporation. All rights reserved.
#

if (MSVC)
	add_custom_command(OUTPUT 				phonon_unity.html
					   DEPENDS				phonon_unity.md pandoc.css
					   COMMAND 				pandoc -c pandoc.css --toc --toc-depth=6 --self-contained -o phonon_unity.html phonon_unity.md
					   WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
					   COMMENT				"Generating HTML documentation for Unity plugin..."
					   VERBATIM)

    add_custom_target(unitydocs ALL DEPENDS phonon_unity.html)
endif ()