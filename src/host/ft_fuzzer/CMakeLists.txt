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
)

install(
	TARGETS
		confuzz
	DESTINATION bin
)