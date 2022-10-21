add_library(ConInternalStubs)
target_sources(ConInternalStubs
	PRIVATE
		stubs.cpp
)

target_precompile_headers(ConInternalStubs
	PRIVATE
		precomp.h
)
