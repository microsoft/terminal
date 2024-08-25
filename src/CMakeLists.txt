find_package(Microsoft.GSL CONFIG REQUIRED)

find_path(GSL_INCLUDE_DIR NAMES gsl)
find_path(FMT_INCLUDE_DIR NAMES format.h)

include_directories(
	inc
	../packages/Microsoft.Windows.ImplementationLibrary.1.0.240122.1/include
	${GSL_INCLUDE_DIR}
	${FMT_INCUDE_DIR}
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
