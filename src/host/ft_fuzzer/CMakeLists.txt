add_executable(confuzz)

target_sources(confuzz
	PRIVATE
		fuzzmain.cpp
		../exe/Host.EXE.rc
		../exe/openconsole.exe.manifest
)

target_include_directories(confuzz
	PRIVATE
		.
        ..
)

target_link_libraries(confuzz
	PRIVATE
		ConHostLib
		winunixptybridge
		delayimp.lib
)

install(
	TARGETS
		confuzz
	DESTINATION bin
)

target_link_options(confuzz PRIVATE /DELAYLOAD:icu.dll)

add_subdirectory(PtyBridge)