set(TEMPLATES_DIR ${CMAKE_SOURCE_DIR}/cmake/templates)
set(MANIFEST_LIB ${TEMPLATES_DIR}/lib.manifest.in)
set(MANIFEST_APP ${TEMPLATES_DIR}/app.manifest.in)

# Generate dependencies for Windows Manifest files
function(gen_manifest_dependencies DEPENDENCY_LIST OUT_VAR)
    set(DEPENDENCY_BLOCK "")

    foreach(DEP_PAIR IN LISTS DEPENDENCY_LIST)
        string(REGEX MATCH "([^:]+):([0-9]+\\.[0-9]+\\.[0-9]+)" MATCHES ${DEP_PAIR})
        set(DEP_NAME ${CMAKE_MATCH_1})
        set(DEP_VERSION ${CMAKE_MATCH_2})

        string(APPEND DEPENDENCY_BLOCK "    <dependency>\n")
        string(APPEND DEPENDENCY_BLOCK "        <dependentAssembly>\n")
        string(APPEND DEPENDENCY_BLOCK "            <assemblyIdentity type=\"win32\" name=\"lib${DEP_NAME}\" version=\"${DEP_VERSION}.0\" />\n")
        string(APPEND DEPENDENCY_BLOCK "        </dependentAssembly>\n")
        string(APPEND DEPENDENCY_BLOCK "    </dependency>\n")
    endforeach()

    set(${OUT_VAR} "${DEPENDENCY_BLOCK}" PARENT_SCOPE)
endfunction()

function(gen_manifest_lib PACKAGE_NAME PACKAGE_VERSION)
    set(PACKAGE_OUT ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/lib${PACKAGE_NAME}.manifest)
    configure_file(${MANIFEST_LIB} ${PACKAGE_OUT})
endfunction()

function(gen_manifest_app PACKAGE_NAME PACKAGE_VERSION)
    get_target_property(TARGET_PATH ${PACKAGE_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(PACKAGE_OUT ${TARGET_PATH}/${PACKAGE_NAME}.exe.manifest)
    gen_manifest_dependencies("${ARGN}" PACKAGE_DEPENDENCIES)
    configure_file(${MANIFEST_APP} ${PACKAGE_OUT})
endfunction()

function(gen_app_config)
    get_target_property(TARGET_PATH ${PROJECT_NAME} RUNTIME_OUTPUT_DIRECTORY)
    set(OUT_PATH ${TARGET_PATH}/${PROJECT_NAME}.exe.config)
    file(RELATIVE_PATH LIBRARY_PATH "${TARGET_PATH}" "${APP_LIB_DIR}")
    configure_file(${TEMPLATES_DIR}/app.config.in ${OUT_PATH})
endfunction()
