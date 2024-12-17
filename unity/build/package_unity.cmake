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

foreach (Unity_PACKAGE IN ITEMS SteamAudio SteamAudioFMODStudio SteamAudioWwise)
	execute_process(
		COMMAND cmake -E copy ${CPACK_PACKAGE_DIRECTORY}/${Unity_PACKAGE}.unitypackage ${CPACK_PACKAGE_DIRECTORY}/../bin/unity
		COMMAND_ECHO STDOUT
	)

	execute_process(
		COMMAND cmake -E rm ${CPACK_PACKAGE_DIRECTORY}/${Unity_PACKAGE}.unitypackage
		COMMAND_ECHO STDOUT
	)
endforeach()
