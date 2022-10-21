add_library(ConRenderAtlas)
target_sources(ConRenderAtlas
	PRIVATE
		AtlasEngine.api.cpp
		AtlasEngine.cpp
		AtlasEngine.r.cpp
		dwrite.cpp
		DWriteTextAnalysis.cpp
		pch.cpp
)

target_precompile_headers(ConRenderAtlas
	PRIVATE
		precomp.h
)
