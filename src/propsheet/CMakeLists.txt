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
)

target_include_directories(console
	PRIVATE
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
