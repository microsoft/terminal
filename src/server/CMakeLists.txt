add_library(ConServer)
target_sources(ConServer
	PRIVATE
		ApiDispatchers.cpp
		ApiDispatchersInternal.cpp
		ApiMessage.cpp
		ApiMessageState.cpp
		ApiSorter.cpp
		ConDrvDeviceComm.cpp
		ConsoleShimPolicy.cpp
		DeviceHandle.cpp
		Entrypoints.cpp
		IoDispatchers.cpp
		IoSorter.cpp
		ObjectHandle.cpp
		ObjectHeader.cpp
		ProcessHandle.cpp
		ProcessList.cpp
		ProcessPolicy.cpp
		WaitBlock.cpp
		WaitQueue.cpp
		WinNTControl.cpp
)

target_precompile_headers(ConServer
	PRIVATE
		precomp.h
)

# We need the interface import for ConCOMProxy to get the MIDL header path
target_link_libraries(ConServer
	PRIVATE
		ConCOMProxy
)
