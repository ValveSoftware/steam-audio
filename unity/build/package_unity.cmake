# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

if (NOT Unity_EXECUTABLE)
	set(Unity_EXECUTABLE unity)
endif()

execute_process(
	COMMAND ${Unity_EXECUTABLE} -batchmode -quit -nographics -logfile - -projectPath ${CPACK_PACKAGE_DIRECTORY}/../src/project/SteamAudioUnity -executeMethod SteamAudio.Build.BuildSteamAudio ${CPACK_PACKAGE_DIRECTORY}
	COMMAND_ECHO STDOUT
)

execute_process(
	COMMAND cmake -E make_directory ${CPACK_PACKAGE_DIRECTORY}/../bin/unity
	COMMAND_ECHO STDOUT
)

execute_process(
	COMMAND cmake -E copy ${CPACK_PACKAGE_DIRECTORY}/SteamAudio.unitypackage ${CPACK_PACKAGE_DIRECTORY}/../bin/unity
	COMMAND_ECHO STDOUT
)

execute_process(
	COMMAND cmake -E rm ${CPACK_PACKAGE_DIRECTORY}/SteamAudio.unitypackage
	COMMAND_ECHO STDOUT
)
