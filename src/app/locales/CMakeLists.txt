set(LOCALE_OUT_DIR ${CMAKE_SOURCE_DIR}/locales)

file(GLOB_RECURSE LOCALES_SRC "*.po")

foreach(LOCALE_FILE ${LOCALES_SRC})
    get_filename_component(FILE_NAME "${LOCALE_FILE}" NAME_WE)
    get_filename_component(DIR_NAME_ABSOLUTE "${LOCALE_FILE}" DIRECTORY)
    get_filename_component(LANG "${DIR_NAME_ABSOLUTE}" NAME)
    set(LOCALE_OUTPUT "${LOCALE_OUT_DIR}/${LANG}/LC_MESSAGES/${FILE_NAME}.mo")
    add_custom_command(
        OUTPUT ${LOCALE_OUTPUT}
        COMMAND msgfmt ${LOCALE_FILE} --output-file="${LOCALE_OUTPUT}"
        COMMENT "Generate ${LANG} locale: ${LOCALE_OUTPUT}"
        DEPENDS ${LOCALE_FILE})
    list(APPEND LOCALE_FILES ${LOCALE_OUTPUT})
endforeach(LOCALE_FILE)

add_custom_target(LOCALES DEPENDS ${LOCALE_FILES})