
include(${ALWF_ROOT_DIR}/cmake/utils.cmake)

set(APP_NAME ${ALWF_APP_NAME})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(USE_ASAN "Use Address Sanitizer" OFF)

add_compile_options(
    "$<$<CONFIG:Debug>:-g;-O0;-Wall>"
    "$<$<CONFIG:Release>:-O3;-DNDEBUG;-fomit-frame-pointer>"
)

set(APP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_LIB_DIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -fexperimental-library")
    add_definitions(-D_LIBCPP_NO_VCRUNTIME)
    add_compile_options(-Wno-vla-extension)

    if(CMAKE_BUILD_TYPE MATCHES Debug AND USE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    endif()
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-flto" HAS_LTO)
    check_cxx_compiler_flag("-s" HAS_S)

    if(HAS_LTO)
        add_compile_options("$<$<CONFIG:Release>:-flto>")
    endif()

    if(HAS_S)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -s")
    endif()
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00)
endif()

include(${ALWF_ROOT_DIR}/cmake/project.cmake)
add_subdirectory(
    "${ALWF_ROOT_DIR}/modules/acul"
    "${CMAKE_CURRENT_BINARY_DIR}/alwf/acul"
)

if(WIN32)
    set(ENABLE_AGRB OFF)
    add_subdirectory(
        "${ALWF_ROOT_DIR}/modules/awin"
        "${CMAKE_CURRENT_BINARY_DIR}/alwf/awin"
        EXCLUDE_FROM_ALL
    )

    if(DEFINED ENV{WEBVIEW2_SDK_PATH})
        include_directories("$ENV{WEBVIEW2_SDK_PATH}/build/native/include")
    else()
        message(FATAL_ERROR "WEBVIEW2_SDK_PATH is not set in the environment.")
    endif()
endif()

add_files(${PROJECT_NAME} RECURSE ${ALWF_SOURCE_DIR})
add_files(${PROJECT_NAME} RECURSE ${CMAKE_CURRENT_LIST_DIR}/src)

add_subdirectory(
    "${ALWF_ROOT_DIR}/modules/ahtt"
    "${CMAKE_CURRENT_BINARY_DIR}/alwf/ahtt"
    EXCLUDE_FROM_ALL
)

# Views
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/templates")
file(GLOB AT_FILES "${ALWF_VIEWS_DIR}/*.at")
set(GENERATED_HEADERS)

foreach(AT_FILE ${AT_FILES})
    get_filename_component(AT_NAME "${AT_FILE}" NAME_WE)
    set(OUT_HEADER "${GENERATED_DIR}/${AT_NAME}.hpp")
    set(DEP_FILE "${GENERATED_DIR}/${AT_NAME}.dep")
    add_custom_command(
        OUTPUT "${OUT_HEADER}"
        COMMAND $<TARGET_FILE:ahtt>
                -i "${AT_FILE}"
                -o "${OUT_HEADER}"
                --base-dir "${ALWF_VIEWS_DIR}"
                --dep-file "${DEP_FILE}"
        DEPENDS ahtt ${AT_FILE}
        DEPFILE "${DEP_FILE}"
        WORKING_DIRECTORY ${APP_LIB_DIR}
        VERBATIM
    )

    list(APPEND GENERATED_HEADERS "${OUT_HEADER}")
endforeach()


add_custom_target(generate_at_templates
    DEPENDS ${GENERATED_HEADERS}
)
list(APPEND DEPENDENT_TARGETS generate_at_templates)

string(TOUPPER "${PROJECT_NAME}" SRC_PREFIX)
add_executable(${PROJECT_NAME} ${${SRC_PREFIX}_SRC})

if(WIN32 AND NOT CMAKE_BUILD_TYPE MATCHES Debug)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)
endif()

# i18n
if(DEFINED ALFW_LOCALES_SRC)
    set(LOCALE_OUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/locales)

    file(GLOB_RECURSE LOCALES_SRC "${ALFW_LOCALES_SRC}/*.po")

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
    list(APPEND DEPENDENT_TARGETS LOCALES)
endif()

# Resources
if(WIN32)
    if(${CMAKE_BUILD_TYPE} MATCHES Release)
        set(RES_SRC ${CMAKE_BINARY_DIR}/resources/app.rc)
        set(APP_RESOURCE ${CMAKE_BINARY_DIR}/resources/app.res)
        set(ICON_PATH ${ALWF_ICON})
        configure_file(${TEMPLATES_DIR}/app.rc.in ${RES_SRC})

        add_custom_command(
            OUTPUT ${APP_RESOURCE}
            COMMAND ${CMAKE_RC_COMPILER} ${RES_SRC} -O coff -o ${APP_RESOURCE}
            DEPENDS ${RES_SRC} ${ALWF_ICON}
            COMMENT "Generate resource: ${APP_RESOURCE}")

        add_custom_target(WIN_RESOURCES DEPENDS ${APP_RESOURCE})
        list(APPEND DEPENDENT_TARGETS WIN_RESOURCES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${APP_RESOURCE})
    endif()
endif(WIN32)

# Frontend
set(ALFW_JS_SRC "${CMAKE_CURRENT_LIST_DIR}/src/alwf.js")
set(ALFW_JS_DST "${ALWF_PUBLIC_DIR}/alwf.js")

add_custom_command(
    OUTPUT "${ALFW_JS_DST}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ALFW_JS_SRC}" "${ALFW_JS_DST}"
    DEPENDS "${ALFW_JS_SRC}"
    COMMENT "Copy alwf.js -> ${ALFW_JS_DST}"
)
add_custom_target(copy_alwf_js DEPENDS "${ALFW_JS_DST}")
list(APPEND DEPENDENT_TARGETS copy_alwf_js)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${ALWF_ROOT_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE acul)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi awin)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WebKit2 REQUIRED webkit2gtk-4.0)

    target_link_libraries(${PROJECT_NAME} PRIVATE ${GTK3_LIBRARIES} ${WebKit2_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GTK3_INCLUDE_DIRS} ${WebKit2_INCLUDE_DIRS})
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS YES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Manifest & config
if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_wv2_arch x64)
    else()
        set(_wv2_arch x86)
    endif()

    set(_WV2_DIR "$ENV{WEBVIEW2_SDK_PATH}/build/native/${_wv2_arch}")
    set(_WV2_DLL "${_WV2_DIR}/WebView2Loader.dll")

    if(EXISTS "${_WV2_DLL}")
        execute_process(
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            "(Get-Item '${_WV2_DLL}').VersionInfo.ProductVersion"
            OUTPUT_VARIABLE WebView2_LoaderVersion
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        set(_WV2_DST "${APP_LIB_DIR}/WebView2Loader.dll")
        add_custom_command(
            OUTPUT "${_WV2_DST}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_WV2_DLL}" "${_WV2_DST}"
            DEPENDS "${_WV2_DLL}"
            COMMENT "Copy WebView2Loader.dll -> ${_WV2_DST}"
        )
        add_custom_target(COPY_WEBVIEW2_DLL DEPENDS "${_WV2_DST}")
        list(APPEND DEPENDENT_TARGETS COPY_WEBVIEW2_DLL)
    endif()

    # Manifest
    set(APP_MANIFEST_DEPS
        libacul:${acul_VERSION}
        libawin:${awin_VERSION}
        WebView2Loader:${WebView2_LoaderVersion}
    )

    gen_manifest_app(${PROJECT_NAME} ${PROJECT_VERSION} ${APP_MANIFEST_DEPS})
    gen_app_config()
endif()

if(DEFINED DEPENDENT_TARGETS)
    add_dependencies(${PROJECT_NAME} ${DEPENDENT_TARGETS})
endif()