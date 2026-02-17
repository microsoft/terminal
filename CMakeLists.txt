cmake_minimum_required(VERSION 3.20)
# We use the BYPRODUCTS feature for improved Ninja integration

if(${CMAKE_VERSION} VERSION_LESS 3.12)
	cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

cmake_policy(SET CMP0092 NEW)

project(conhost
	VERSION 1.0.0
	DESCRIPTION "Windows Console Host"
	LANGUAGES C CXX)

# Some projects (like the COM proxy) generate C files.
list(APPEND CMAKE_CXX_SOURCE_FILE_EXTENSIONS c)

set(CMAKE_CXX_STANDARD 20)

# Hybrid CRT (runtime library is static, crt is dynamic)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>)

execute_process(
	COMMAND pwsh -NoProfile -Command "${CMAKE_SOURCE_DIR}/tools/Generate-FeatureStagingHeader.ps1" -Branding Dev -Path "${CMAKE_CURRENT_LIST_DIR}\\src\\features.xml" -OutputPath "${CMAKE_BINARY_DIR}\\features.h"
)

add_compile_definitions(
	UNICODE
	_UNICODE
)

add_compile_options(
	/W3 /WX /utf-8
	/wd4201 /wd4702
	/permissive-
	/Zc:preprocessor
	/Zc:__cplusplus /Zc:__STDC__ /Zc:enumTypes /Zc:externConstexpr /Zc:templateScope /Zc:throwingNew
	/guard:cf
	/FI${CMAKE_BINARY_DIR}\\features.h
	$<$<NOT:$<CONFIG:DEBUG>>:/GL>
)

add_link_options(
	$<$<NOT:$<CONFIG:DEBUG>>:/LTCG>
	$<$<NOT:$<CONFIG:DEBUG>>:/OPT:REF>
	$<$<NOT:$<CONFIG:DEBUG>>:/OPT:ICF>
	# Hybrid CRT (switch the static crt for the dynaimc one)
	/nodefaultlib:libucrt$<$<CONFIG:Debug>:d>.lib
	/defaultlib:ucrt$<$<CONFIG:Debug>:d>.lib
	/guard:cf
)

add_compile_definitions(
	"$<$<CONFIG:DEBUG>:DEBUG>"
	"$<$<CONFIG:DEBUG>:_DEBUG>"
	"$<$<CONFIG:DEBUG>:DBG>"
)

add_subdirectory(src)
