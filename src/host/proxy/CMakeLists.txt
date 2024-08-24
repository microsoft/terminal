set(IDL_FILES IConsoleHandoff.idl ITerminalHandoff.idl)
add_custom_target(ConCOMProxyIDL DEPENDS ${IDL_FILES})
target_sources(ConCOMProxyIDL
	PRIVATE
		${IDL_FILES}
)

foreach(FILE ${IDL_FILES})
  get_filename_component(FILE_WE ${FILE} NAME_WE)
  add_custom_command(TARGET ConCOMProxyIDL
	  DEPENDS_EXPLICIT_ONLY
	  DEPENDS ${FILE}
	  COMMAND midl.exe /nologo /target NT100 /x64 /out "${CMAKE_CURRENT_BINARY_DIR}" /h "${FILE_WE}.h" ${FILE}
                     MAIN_DEPENDENCY ${FILE}
		     COMMENT "MIDL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
  set_source_files_properties(
	  ${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}_p.c
	  ${CMAKE_CURRENT_BINARY_DIR}/${FILE_WE}_i.c
	  ${CMAKE_CURRENT_BINARY_DIR}/dlldata.c
	  PROPERTIES GENERATED TRUE
  )
endforeach(FILE)

add_library(ConCOMProxy STATIC)
set_target_properties(ConCOMProxy PROPERTIES LINKER_LANGUAGE CXX)
add_dependencies(ConCOMProxy ConCOMProxyIDL)
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
