find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)

get_target_property(WIL_INCLUDE_DIR WIL::WIL INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(GSL_INCLUDE_DIR Microsoft.GSL::GSL INTERFACE_INCLUDE_DIRECTORIES)

find_path(FMT_INCLUDE_DIR NAMES format.h)

include_directories(
	inc
	${WIL_INCLUDE_DIR}
	../oss/stb
	../oss/chromium
	../oss/interval_tree
	../dep/Console
	../dep/NT
	../dep/Win32K
	../dep
)

add_subdirectory(audio)
add_subdirectory(buffer)
add_subdirectory(host)
add_subdirectory(interactivity)
add_subdirectory(propsheet)
add_subdirectory(propslib)
add_subdirectory(renderer)
add_subdirectory(server)
add_subdirectory(terminal)
#add_subdirectory(til)
add_subdirectory(tsf)
add_subdirectory(types)
add_subdirectory(internal)
add_subdirectory(winconpty)
