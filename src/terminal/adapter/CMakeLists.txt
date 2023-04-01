add_library(ConTermAdapt)
target_sources(ConTermAdapt
	PRIVATE
		adaptDispatch.cpp
		adaptDispatchGraphics.cpp
		FontBuffer.cpp
		MacroBuffer.cpp
		InteractDispatch.cpp
		telemetry.cpp
		terminalOutput.cpp
		tracing.cpp
)

target_precompile_headers(ConTermAdapt
	PRIVATE
		precomp.h
)
