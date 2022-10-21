add_library(ConTSF)
target_sources(ConTSF
	PRIVATE
		ConsoleTSF.cpp
		contsf.cpp
		TfCatUtil.cpp
		TfConvArea.cpp
		TfDispAttr.cpp
		TfEditSession.cpp
		TfTxtevCb.cpp
)

target_precompile_headers(ConTSF
	PRIVATE
		precomp.h
)
