cmake_minimum_required(VERSION 3.1)

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
	/FI${CMAKE_CURRENT_LIST_DIR}\\feature.h
)

add_compile_definitions(
	"$<$<CONFIG:DEBUG>:DEBUG>"
	"$<$<CONFIG:DEBUG>:_DEBUG>"
	"$<$<CONFIG:DEBUG>:DBG>"
)

add_subdirectory(src)
