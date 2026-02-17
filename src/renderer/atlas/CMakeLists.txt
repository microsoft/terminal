add_library(ConRenderAtlas)

target_include_directories(ConRenderAtlas
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>
)

target_sources(ConRenderAtlas
	PRIVATE
		AtlasEngine.api.cpp
		AtlasEngine.cpp
		AtlasEngine.r.cpp
		Backend.cpp
		BackendD2D.cpp
		BackendD3D.cpp
		BuiltinGlyphs.cpp
		dwrite_helpers.cpp
		DWriteTextAnalysis.cpp
		stb_rect_pack.cpp
		wic.cpp
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shader_vs.h
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shader_ps.h
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/custom_shader_vs.h
		${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/custom_shader_ps.h
)

target_precompile_headers(ConRenderAtlas
	PRIVATE
		pch.h
)

set(HLSL_SHADER_FILES shader_vs.hlsl shader_ps.hlsl custom_shader_ps.hlsl custom_shader_vs.hlsl)

set_source_files_properties(shader_vs.hlsl PROPERTIES ShaderType "vs")
set_source_files_properties(shader_ps.hlsl PROPERTIES ShaderType "ps")
set_source_files_properties(custom_shader_vs.hlsl PROPERTIES ShaderType "vs")
set_source_files_properties(custom_shader_ps.hlsl PROPERTIES ShaderType "ps")
set_source_files_properties(${HLSL_SHADER_FILES} PROPERTIES ShaderModel "4_0")

foreach(FILE ${HLSL_SHADER_FILES})
	get_filename_component(FILE_WE ${FILE} NAME_WE)
	get_source_file_property(shadertype ${FILE} ShaderType)
	get_source_file_property(shadermodel ${FILE} ShaderModel)
	add_custom_command(
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}.h
		COMMAND fxc.exe /nologo /Emain /Vn${FILE_WE} /Zi /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O3> /Zsb /Fh ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}.h /Fd ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${FILE_WE}.pdb ${FILE}
		MAIN_DEPENDENCY ${FILE}
		COMMENT "HLSL ${FILE}"
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)
endforeach(FILE)
