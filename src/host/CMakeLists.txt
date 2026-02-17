add_library(ConHostLib)
target_sources(ConHostLib
	PRIVATE
		_output.cpp
		_stream.cpp
		AccessibilityNotifier.cpp
		alias.cpp
		cmdline.cpp
		ConsoleArguments.cpp
		consoleInformation.cpp
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
		tracing.cpp
		utils.cpp
		VtInputThread.cpp
		VtIo.cpp
		writeData.cpp
)

target_include_directories(ConHostLib
	PRIVATE
		.
		"$<TARGET_PROPERTY:ConCOMProxy,INTERFACE_INCLUDE_DIRECTORIES>"
)

target_precompile_headers(ConHostLib
	PRIVATE
		precomp.h
)

find_package(fmt CONFIG REQUIRED)

target_link_libraries(ConHostLib
	INTERFACE
		ConCOMProxy
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
		ConRenderAtlas

		ConTypes

		fmt::fmt

		onecoreuap_apiset.lib
		usp10.lib
		imm32.lib
		winmm.lib
		ntdll.lib
)

add_subdirectory(proxy)
add_subdirectory(exe)