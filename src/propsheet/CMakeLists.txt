add_library(console SHARED)
target_sources(console
	PRIVATE
		ColorControl.cpp
		ColorsPage.cpp
		console.cpp
		dbcs.cpp
		dll.cpp
		fontdlg.cpp
		globals.cpp
		init.cpp
		LayoutPage.cpp
		misc.cpp
		OptionsPage.cpp
		preview.cpp
		PropSheetHandler.cpp
		registry.cpp
		TerminalPropsheetPage.cpp
		util.cpp

		${CMAKE_CURRENT_BINARY_DIR}/strid.h

		console.rc
		console.def
)

target_include_directories(console
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}
		../host
)

target_link_libraries(console
	PRIVATE
		ConInternalStubs
		ConPropsLib
		ConTypes

		onecoreuap_apiset.lib
		shlwapi.lib
)

target_precompile_headers(console
	PRIVATE
		precomp.h
)

add_custom_command(
	DEPENDS strid.mc
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/strid.rc
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/strid.h
	COMMAND mc.exe -r "${CMAKE_CURRENT_BINARY_DIR}" -h "${CMAKE_CURRENT_BINARY_DIR}" strid.mc
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

install(
	TARGETS
		console
	DESTINATION bin
)