add_library(ConRenderGdi)
target_sources(ConRenderGdi
	PRIVATE
		invalidate.cpp
		math.cpp
		paint.cpp
		state.cpp
)

target_precompile_headers(ConRenderGdi
	PRIVATE
		precomp.h
)
