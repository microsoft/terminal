add_library(ConRenderBase)
target_sources(ConRenderBase
	PRIVATE
		fontinfo.cpp
		FontInfoBase.cpp
		FontInfoDesired.cpp
		FontResource.cpp
		RenderEngineBase.cpp
		renderer.cpp
		RenderSettings.cpp
		thread.cpp
)

target_precompile_headers(ConRenderBase
	PRIVATE
		precomp.h
)
