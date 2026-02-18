add_executable(conhost WIN32)

target_sources(conhost
	PRIVATE
		CConsoleHandoff.cpp
		exemain.cpp
		Host.EXE.rc
		OpenConsole.exe.manifest
)

target_include_directories(conhost
	PRIVATE
		.
		..
)

target_link_libraries(conhost
	PRIVATE
		ConHostLib
		ConCOMProxy
)

target_link_options(conhost PRIVATE /entry:wWinMainCRTStartup)

target_precompile_headers(conhost
	PRIVATE
		../precomp.h
)

install(
	TARGETS
		conhost
	DESTINATION bin
)