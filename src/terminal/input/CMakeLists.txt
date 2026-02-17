add_library(ConTermInput)
target_sources(ConTermInput
	PRIVATE
		mouseInput.cpp
		terminalInput.cpp
)

target_precompile_headers(ConTermInput
	PRIVATE
		precomp.h
)
