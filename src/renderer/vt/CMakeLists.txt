add_library(ConRenderVt)
target_sources(ConRenderVt
	PRIVATE
		invalidate.cpp
		math.cpp
		paint.cpp
		state.cpp
		tracing.cpp
		VtSequences.cpp
		Xterm256Engine.cpp
		XtermEngine.cpp
)

target_precompile_headers(ConRenderVt
	PRIVATE
		precomp.h
)
