add_library(winunixptybridge SHARED)
target_sources(winunixptybridge
	PRIVATE
                file.cpp
                bridge.def
)

target_include_directories(winunixptybridge
	PUBLIC
                PtyBridge.h
)

target_compile_definitions(winunixptybridge
        PRIVATE
                "PTYB_IMPEXP=__declspec(dllexport)"
)