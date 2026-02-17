set(IDL_FILES IConsoleHandoff.idl ITerminalHandoff.idl)

foreach(FILE ${IDL_FILES})
	get_filename_component(FILE_WE ${FILE} NAME_WE)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}_p.c
		BYPRODUCTS
			${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}_i.c
			${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}.h
			${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/dlldata.c
		MAIN_DEPENDENCY ${FILE}
		COMMAND midl.exe /nologo /target NT100 /x64 /out "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>" /h "${FILE_WE}.h" ${FILE}
		COMMENT "MIDL ${FILE}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM)
endforeach(FILE)

set_source_files_properties(
	${CMAKE_CURRENT_BINARY_DIR}/Debug/dlldata.c
	${CMAKE_CURRENT_BINARY_DIR}/Release/dlldata.c
	${CMAKE_CURRENT_BINARY_DIR}/RelWithDebInfo/dlldata.c
	PROPERTIES GENERATED TRUE
)

add_library(ConCOMProxy STATIC)
set_target_properties(ConCOMProxy PROPERTIES LINKER_LANGUAGE CXX)
target_sources(ConCOMProxy
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ITerminalHandoff_p.c
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/ITerminalHandoff_i.c
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/IConsoleHandoff_p.c
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/IConsoleHandoff_i.c
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/dlldata.c
)

target_include_directories(ConCOMProxy
	INTERFACE
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
)
