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

include(FindPackageHandleStandardArgs)

find_package(Python3)

if (Python3_FOUND)
    find_path(Wwise_INCLUDE_DIR
        NAMES           AkPlatforms.h
        PATHS           $ENV{WWISEROOT}/SDK ${CMAKE_HOME_DIRECTORY}/include/wwise/SDK
        PATH_SUFFIXES   include/AK
    )

    find_file(Wwise_WP_PY
        NAMES   wp.py
        PATHS   ${Wwise_INCLUDE_DIR}/../../../Scripts/Build/Plugins
    )
endif()

find_package_handle_standard_args(Wwise
    FOUND_VAR       Wwise_FOUND
    REQUIRED_VARS   Wwise_INCLUDE_DIR Wwise_WP_PY
)

if (Wwise_FOUND AND NOT TARGET Wwise:Wwise)
    add_library(Wwise::Wwise UNKNOWN IMPORTED)
    target_include_directories(Wwise::Wwise INTERFACE ${Wwise_INCLUDE_DIR})
endif()

mark_as_advanced(Wwise_INCLUDE_DIR Wwise_WP_PY)
