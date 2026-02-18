add_library(conpty SHARED)
target_sources(conpty
	PRIVATE
		winconpty.cpp
		../server/DeviceHandle.cpp
		../server/WinNTControl.cpp
)

target_link_libraries(conpty
	PRIVATE
		onecoreuap_apiset.lib
)

target_precompile_headers(conpty
	PRIVATE
		precomp.h
)

install(
	TARGETS
		conpty
	DESTINATION lib
)
