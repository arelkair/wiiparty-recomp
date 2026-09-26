if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

get_filename_component(OUTPUT_DIR "${EXECUTABLE}" DIRECTORY)

find_program(WINDEPLOYQT NAMES windeployqt6 windeployqt HINTS "${QT_BIN}")
if(WINDEPLOYQT)
    execute_process(COMMAND "${WINDEPLOYQT}" --no-translations --no-system-d3d-compiler --no-opengl-sw "${EXECUTABLE}"
                    OUTPUT_QUIET ERROR_QUIET)
endif()

file(GLOB DEPLOYED_LIBRARIES "${OUTPUT_DIR}/*.dll" "${OUTPUT_DIR}/*/*.dll")
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${EXECUTABLE}"
    LIBRARIES ${DEPLOYED_LIBRARIES}
    DIRECTORIES "${QT_BIN}"
    RESOLVED_DEPENDENCIES_VAR RESOLVED
    CONFLICTING_DEPENDENCIES_PREFIX CONFLICTS
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_INCLUDE_REGEXES "^${QT_BIN}"
    POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss]ystem32.*")
foreach(LIBRARY ${RESOLVED})
    string(FIND "${LIBRARY}" "${QT_BIN}" INSIDE)
    if(INSIDE EQUAL 0)
        file(COPY "${LIBRARY}" DESTINATION "${OUTPUT_DIR}")
    endif()
endforeach()
