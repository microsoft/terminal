add_library(ConPropsLib)
target_sources(ConPropsLib
	PRIVATE
		DelegationConfig.cpp
		RegistrySerialization.cpp
		ShortcutSerialization.cpp
		TrueTypeFontList.cpp
)

target_precompile_headers(ConPropsLib
	PRIVATE
		precomp.h
)
