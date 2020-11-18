function(add_shader TARGET SHADER)
	# Find glslc shader compiler.
	find_program(GLSLC glslc)

	get_filename_component(SHADER_FILE ${SHADER} NAME)
	set(OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_FILE}.spv)

	# Add a custom command to compile GLSL to SPIR-V.
	get_filename_component(OUTPUT_DIR ${OUTPUT_PATH} DIRECTORY)
	file(MAKE_DIRECTORY ${OUTPUT_DIR})
	add_custom_command(
		OUTPUT ${OUTPUT_PATH}
		COMMAND ${GLSLC} -g -o ${OUTPUT_PATH} ${SHADER}
		DEPENDS ${SHADER}
		IMPLICIT_DEPENDS CXX ${SHADER}
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM)

	# Make sure our native build depends on this output.
	set_source_files_properties(${OUTPUT_PATH} PROPERTIES GENERATED TRUE)
	target_sources(${TARGET} PRIVATE ${OUTPUT_PATH})
endfunction(add_shader)
