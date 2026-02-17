add_library(ConRenderBase)
target_sources(ConRenderBase
	PRIVATE
		CSSLengthPercentage.cpp
		fontinfo.cpp
		FontInfoBase.cpp
		FontInfoDesired.cpp
		FontResource.cpp
		RenderEngineBase.cpp
		renderer.cpp
		RenderSettings.cpp
)

target_precompile_headers(ConRenderBase
	PRIVATE
		precomp.h
)
