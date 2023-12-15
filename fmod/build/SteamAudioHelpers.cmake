# Copyright 2017 Valve Corporation. All rights reserved. Subject to the following license:
# https://valvesoftware.github.io/steam-audio/license.html

function (get_local_lib_path LIB_PATH)
	if (IPL_OS_WINDOWS)
		if (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/windows-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/windows-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_LINUX)
		if (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/linux-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/linux-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_MACOS)
		set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/osx PARENT_SCOPE)
	elseif (IPL_OS_ANDROID)
		if (IPL_CPU_ARMV7)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-armv7 PARENT_SCOPE)
		elseif (IPL_CPU_ARMV8)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-armv8 PARENT_SCOPE)
		elseif (IPL_CPU_X86)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/android-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_IOS)
		set(${LIB_PATH} ${CMAKE_HOME_DIRECTORY}/lib/ios PARENT_SCOPE)
	endif()
endfunction()

function (get_bin_subdir BIN_SUBDIR)
	if (IPL_OS_WINDOWS)
		if (IPL_CPU_X86)
			set(${BIN_SUBDIR} windows-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} windows-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_LINUX)
		if (IPL_CPU_X86)
			set(${BIN_SUBDIR} linux-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} linux-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_MACOS)
		set(${BIN_SUBDIR} osx PARENT_SCOPE)
	elseif (IPL_OS_ANDROID)
		if (IPL_CPU_ARMV7)
			set(${BIN_SUBDIR} android-armv7 PARENT_SCOPE)
		elseif (IPL_CPU_ARMV8)
			set(${BIN_SUBDIR} android-armv8 PARENT_SCOPE)
		elseif (IPL_CPU_X86)
			set(${BIN_SUBDIR} android-x86 PARENT_SCOPE)
		elseif (IPL_CPU_X64)
			set(${BIN_SUBDIR} android-x64 PARENT_SCOPE)
		endif()
	elseif (IPL_OS_IOS)
		set(${BIN_SUBDIR} ios PARENT_SCOPE)
	endif()
endfunction()

function (add_executable_or_apk name)
  if (IPL_OS_ANDROID)
    add_library(${name} SHARED ${ARGN})
    if (IPL_CPU_ARMV7)
      set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY          "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a"
      )
      add_custom_command(TARGET ${name} POST_BUILD
      	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/armeabi-v7a
        COMMAND gradlew --offline --no-daemon ${name}:assembleArmv7 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/android
      )
    elseif (IPL_CPU_X86)
      set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY          "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86"
      )
      add_custom_command(TARGET ${name} POST_BUILD
      	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86
        COMMAND gradlew --offline --no-daemon ${name}:assembleX86 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/android
      )
    elseif (IPL_CPU_X64)
      set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY          "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86_64"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86_64"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86_64"
      )
      add_custom_command(TARGET ${name} POST_BUILD
      	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/x86_64
        COMMAND gradlew --offline --no-daemon ${name}:assembleX64 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/android
      )
    elseif (IPL_CPU_ARMV8)
      set_target_properties(${name} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY          "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/arm64-v8a"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/arm64-v8a"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/arm64-v8a"
      )
      add_custom_command(TARGET ${name} POST_BUILD
      	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_HOME_DIRECTORY}/build/android/${name}/src/main/jniLibs/arm64-v8a
        COMMAND gradlew --offline --no-daemon ${name}:assembleArm64 WORKING_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build/android
      )
    endif()
  else()
    add_executable(${name} ${ARGN})
	if (IPL_OS_IOS)
		set_target_properties(${name} PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER com.valve.${name})
		set_target_properties(${name} PROPERTIES MACOSX_BUNDLE_BUNDLE_VERSION 1.0.0)
		set_target_properties(${name} PROPERTIES MACOSX_BUNDLE_SHORT_VERSION_STRING 1.0.0)
	endif()
  endif()
endfunction()

function (install_executable_or_apk name dir)
	if (IPL_OS_ANDROID)
		if (IPL_CPU_ARMV7)
			set(arch armv7)
		elseif (IPL_CPU_ARMV8)
			set(arch arm64)
		elseif (IPL_CPU_X86)
			set(arch x86)
		elseif (IPL_CPU_X64)
			set(arch x64)
		endif()
		install(FILES ${CMAKE_HOME_DIRECTORY}/build/android/${name}/build/outputs/apk/${name}-${arch}-debug.apk DESTINATION ${dir} RENAME ${name}.apk)
	elseif (IPL_OS_IOS)
		install(TARGETS ${name} BUNDLE DESTINATION ${dir})
	else()
		install(TARGETS ${name} RUNTIME DESTINATION ${dir})
	endif()
endfunction()
