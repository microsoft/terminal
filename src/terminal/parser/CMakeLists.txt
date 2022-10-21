add_library(ConTermParser)
target_sources(ConTermParser
	PRIVATE
		base64.cpp
		InputStateMachineEngine.cpp
		OutputStateMachineEngine.cpp
		stateMachine.cpp
		telemetry.cpp
		tracing.cpp
)

target_precompile_headers(ConTermParser
	PRIVATE
		precomp.h
)
