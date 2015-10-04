#This code is taken from here:
#http://www.mail-archive.com/cmake@cmake.org/msg04394.html

IF(CMAKE_COMPILER_IS_GNUCXX)
	EXEC_PROGRAM(
		${CMAKE_CXX_COMPILER}
		ARGS                    --version
		OUTPUT_VARIABLE _compiler_output)
	STRING(REGEX REPLACE ".* ([0-9]\\.[0-9]\\.[0-9]) .*" "\\1"
		   gcc_compiler_version ${_compiler_output})
	#MESSAGE("GCC Version: ${gcc_compiler_version}")
	IF(gcc_compiler_version MATCHES "4\\.[0-9]\\.[0-9]")
		MESSAGE(STATUS "We have support for precompiled headers")
		SET(PCHSupport_FOUND TRUE)
	ELSE()
		IF(gcc_compiler_version MATCHES "3\\.4\\.[0-9]")
			MESSAGE(STATUS "We have support for precompiled headers")
			SET(PCHSupport_FOUND TRUE)
		ENDIF()
	ENDIF()
ENDIF()

MACRO(ADD_PRECOMPILED_HEADER _targetName _input )
	#get the file name (no path)
	GET_FILENAME_COMPONENT(_name ${_input} NAME_WE)

#	GET_DIRECTORY_PROPERTY(__allvars VARIABLES)
#	#MESSAGE(STATUS "VARIABLES on ${_targetName}: ${__allvars}")
#
#	MESSAGE(STATUS "--------------------------------------")
#	MESSAGE(STATUS "TARGET: ${_targetName}")
#	FOREACH(item ${__allvars})
#		MESSAGE(STATUS "${item}: ${${item}}")
#	ENDFOREACH()
#	MESSAGE(STATUS "--------------------------------------")
#	MESSAGE(FATAL_ERROR "asdasdsa")

	#locate the file
	SET(_source "${_input}")

	#compute the output directory
	SET(_outdir "${CMAKE_BINARY_DIR}/precompiled.gch/${CMAKE_BUILD_TYPE}.c++")

	#create the output directory
	MAKE_DIRECTORY(${_outdir})

	#compute the output file
	SET(_output "${_outdir}/${_name}.gch")

	#get the compiler flags
	STRING(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" _flags_var_name)
	SET(_compiler_FLAGS ${CMAKE_CXX_FLAGS} ${${_flags_var_name}})

	#get the list of include directories for the current project and compute the new version of compiler flags by
	#appinding -I
	GET_DIRECTORY_PROPERTY(_directory_flags INCLUDE_DIRECTORIES)
	FOREACH(item ${_directory_flags})
		LIST(APPEND _compiler_FLAGS "-I${item}")
	ENDFOREACH()

	#get the definitions of the current project and append them as well to the current compiler flags
	GET_DIRECTORY_PROPERTY(_directory_flags COMPILE_DEFINITIONS)
	FOREACH(item ${_directory_flags})
		LIST(APPEND _compiler_FLAGS "-D${item}")
	ENDFOREACH()

	#now split the entire thing
	SEPARATE_ARGUMENTS(_compiler_FLAGS)

	#compute the new command
	ADD_CUSTOM_COMMAND(
		OUTPUT ${_output}
		COMMAND ${CMAKE_CXX_COMPILER} ${PCH_CXX_FLAGS} ${_compiler_FLAGS} -x c++-header -c -o ${_output} ${_source}
		DEPENDS ${_source}
	)

	#add a new target to the project
	ADD_CUSTOM_TARGET(${_targetName}_${_name}_gch DEPENDS ${_output})
	ADD_DEPENDENCIES(${_targetName} ${_targetName}_${_name}_gch)

	EXECUTE_PROCESS(COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE _COMPILER_VERSION)
	STRING(TOLOWER "${_COMPILER_VERSION}" _COMPILER_VERSION)
	IF(_COMPILER_VERSION MATCHES ".*(clang|llvm).*")
		SET_TARGET_PROPERTIES(${_targetName} PROPERTIES COMPILE_FLAGS "-include-pch ${_output} -Winvalid-pch")
	ELSE()
		SET_TARGET_PROPERTIES(${_targetName} PROPERTIES COMPILE_FLAGS "-I${_outdir} -include ${_name} -Winvalid-pch")
	ENDIF()
ENDMACRO()
