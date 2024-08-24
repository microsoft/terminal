add_library(ConTSF)
target_sources(ConTSF
	PRIVATE
		Handle.cpp
		Implementation.cpp
)

target_precompile_headers(ConTSF
	PRIVATE
		precomp.h
)
