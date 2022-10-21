add_executable(conhost WIN32)
target_sources(conhost
	PRIVATE
		_output.cpp
		_stream.cpp
		alias.cpp
		cmdline.cpp
		CommandListPopup.cpp
		CommandNumberPopup.cpp
		conareainfo.cpp
		conimeinfo.cpp
		ConsoleArguments.cpp
		consoleInformation.cpp
		convarea.cpp
		CopyFromCharPopup.cpp
		CopyToCharPopup.cpp
		CursorBlinker.cpp
		dbcs.cpp
		directio.cpp
		getset.cpp
		globals.cpp
		handle.cpp
		history.cpp
		init.cpp
		input.cpp
		inputBuffer.cpp
		inputKeyInfo.cpp
		inputReadHandleData.cpp
		misc.cpp
		ntprivapi.cpp
		output.cpp
		outputStream.cpp
		popup.cpp
		PtySignalInputThread.cpp
		readData.cpp
		readDataCooked.cpp
		readDataDirect.cpp
		readDataRaw.cpp
		registry.cpp
		renderData.cpp
		renderFontDefaults.cpp
		screenInfo.cpp
		scrolling.cpp
		selection.cpp
		selectionInput.cpp
		selectionState.cpp
		settings.cpp
		srvinit.cpp
		stream.cpp
		telemetry.cpp
		tracing.cpp
		utf8ToWideCharParser.cpp
		utils.cpp
		VtApiRoutines.cpp
		VtInputThread.cpp
		VtIo.cpp
		writeData.cpp

		exe/CConsoleHandoff.cpp
		exe/exemain.cpp
)

target_include_directories(conhost
	PRIVATE
		.
)

target_link_libraries(conhost
	PRIVATE
		ConAudioMidi
		ConBufferOut
		ConInteractivityBase
		ConInteractivityWin32
		ConInternalStubs
		ConPropsLib
		ConTermInput
		ConTermParser
		ConTermAdapt
		ConTSF
		ConServer

		ConRenderBase
		ConRenderGdi
		ConRenderVt

		ConTypes

		onecoreuap_apiset.lib
		usp10.lib
		winmm.lib
)

target_link_options(conhost PRIVATE /entry:wWinMainCRTStartup)

target_precompile_headers(conhost
	PRIVATE
		precomp.h
)
