add_library(ConInteractivityBase)
target_sources(ConInteractivityBase
	PRIVATE
		ApiDetector.cpp
		EventSynthesis.cpp
		HostSignalInputThread.cpp
		InteractivityFactory.cpp
		RemoteConsoleControl.cpp
		ServiceLocator.cpp
		VtApiRedirection.cpp
		PseudoConsoleWindowAccessibilityProvider.cpp
)

target_precompile_headers(ConInteractivityBase
	PRIVATE
		precomp.h
)
