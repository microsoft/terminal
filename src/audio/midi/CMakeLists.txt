add_library(ConAudioMidi)
target_sources(ConAudioMidi
	PRIVATE
		MidiAudio.cpp
)

target_precompile_headers(ConAudioMidi
	PRIVATE
		precomp.h
)
