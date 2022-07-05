# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

include(FindPackageHandleStandardArgs)

find_program(Unreal_EXECUTABLE
	NAMES UnrealBuildTool.exe
	PATHS ${Unreal_EXECUTABLE_DIR}
)

find_package_handle_standard_args(Unreal 
	FOUND_VAR 		Unreal_FOUND 
	REQUIRED_VARS 	Unreal_EXECUTABLE
)

mark_as_advanced(Unreal_EXECUTABLE)
