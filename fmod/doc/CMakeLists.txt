#
# Copyright (c) Valve Corporation. All rights reserved.
#

if (MSVC)
	add_custom_command(OUTPUT 				phonon_fmod.html
					   DEPENDS				phonon_fmod.md pandoc.css
					   COMMAND 				pandoc -c pandoc.css --toc --toc-depth=6 --self-contained -o phonon_fmod.html phonon_fmod.md
					   WORKING_DIRECTORY	${CMAKE_CURRENT_SOURCE_DIR}
					   COMMENT				"Generating HTML documentation for FMOD Studio plugin..."
					   VERBATIM)

    add_custom_target(fmoddocs ALL DEPENDS phonon_fmod.html)
endif ()