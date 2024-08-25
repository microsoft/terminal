set(IDL_FILES IConsoleHandoff.idl ITerminalHandoff.idl)

foreach(FILE ${IDL_FILES})
	get_filename_component(FILE_WE ${FILE} NAME_WE)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}_p.c
		BYPRODUCTS
			${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}_i.c
			${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}.h
		MAIN_DEPENDENCY ${FILE}
		COMMAND midl.exe /nologo /target NT100 /x64 /out "${CMAKE_CURRENT_BINARY_DIR}" /h "${FILE_WE}.h" ${FILE}
		COMMENT "MIDL ${FILE}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM)
endforeach(FILE)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/dlldata.c
	PROPERTIES GENERATED TRUE
)

add_library(ConCOMProxy STATIC)
set_target_properties(ConCOMProxy PROPERTIES LINKER_LANGUAGE CXX)
target_sources(ConCOMProxy
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/ITerminalHandoff_p.c
		${CMAKE_CURRENT_BINARY_DIR}/ITerminalHandoff_i.c
		${CMAKE_CURRENT_BINARY_DIR}/IConsoleHandoff_p.c
		${CMAKE_CURRENT_BINARY_DIR}/IConsoleHandoff_i.c
		${CMAKE_CURRENT_BINARY_DIR}/dlldata.c
)

target_include_directories(ConCOMProxy
	INTERFACE
		${CMAKE_CURRENT_BINARY_DIR}
)
