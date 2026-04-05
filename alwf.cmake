if(NOT DEFINED ACBT_LOADED)
    include(${ALWF_ROOT_DIR}/cmake/utils.cmake)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/cmake/windows_sdk_tools.cmake)

if(WIN32 AND NOT DEFINED ALWF_WEBVIEW2_VERSION)
    if(DEFINED ENV{NUGET_PACKAGES} AND NOT "$ENV{NUGET_PACKAGES}" STREQUAL "")
        set(_alwf_webview2_global_packages_root "$ENV{NUGET_PACKAGES}")
    else()
        set(_alwf_webview2_global_packages_root "$ENV{USERPROFILE}/.nuget/packages")
    endif()

    file(GLOB _alwf_webview2_cached_version_dirs
        LIST_DIRECTORIES true
        "${_alwf_webview2_global_packages_root}/microsoft.web.webview2/*")
    list(SORT _alwf_webview2_cached_version_dirs COMPARE NATURAL ORDER DESCENDING)
    list(LENGTH _alwf_webview2_cached_version_dirs _alwf_webview2_cached_version_count)
    if(_alwf_webview2_cached_version_count GREATER 0)
        list(GET _alwf_webview2_cached_version_dirs 0 _alwf_webview2_cached_version_dir)
        get_filename_component(_alwf_webview2_default_version "${_alwf_webview2_cached_version_dir}" NAME)
    else()
        set(_alwf_webview2_default_version "1.0.3405.78")
    endif()

    set(ALWF_WEBVIEW2_VERSION "${_alwf_webview2_default_version}" CACHE STRING
        "Version of Microsoft.Web.WebView2 to install from NuGet")
endif()

set(APP_NAME ${ALWF_APP_NAME})
if(NOT DEFINED ALWF_STATIC_ASSETS_NAME)
    set(ALWF_STATIC_ASSETS_NAME "assets.umlib")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(USE_ASAN "Use Address Sanitizer" OFF)

add_compile_options(
    "$<$<CONFIG:Debug>:-g;-O0;-Wall>"
    "$<$<CONFIG:Release>:-O3;-DNDEBUG;-fomit-frame-pointer>"
)

if(NOT DEFINED APP_LIB_DIR)
    set(APP_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_SYSTEM_NAME})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_LIB_DIR})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${APP_LIB_DIR})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_LIB_DIR})
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    configure_clang_toolchain()
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    enable_lto()
    enable_strip()
endif()

if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00)
endif()

if(NOT DEFINED ACBT_PROJECT_LOADED)
    include(${ALWF_ROOT_DIR}/cmake/project.cmake)
endif()

if(NOT TARGET acul)
    add_subdirectory(
        "${ALWF_ROOT_DIR}/modules/acul"
        "${CMAKE_CURRENT_BINARY_DIR}/alwf/acul"
    )
endif()


if(NOT TARGET umbf)
    add_subdirectory(
        "${ALWF_ROOT_DIR}/modules/umbf"
        "${CMAKE_CURRENT_BINARY_DIR}/alwf/umbf"
        EXCLUDE_FROM_ALL
    )
endif()

if(WIN32 AND NOT TARGET awin)
    set(ENABLE_AGRB OFF)
    add_subdirectory(
        "${ALWF_ROOT_DIR}/modules/awin"
        "${CMAKE_CURRENT_BINARY_DIR}/alwf/awin"
        EXCLUDE_FROM_ALL
    )
endif()

if(WIN32)
    set(alwf_webview2_root_dir "${CMAKE_BINARY_DIR}/webview2-sdk")
    set(alwf_webview2_packages_dir "${alwf_webview2_root_dir}/packages")

    acbt_nuget_install_package(
        PACKAGE_NAME "Microsoft.Web.WebView2"
        PACKAGE_VERSION "${ALWF_WEBVIEW2_VERSION}"
        OUTPUT_DIRECTORY "${alwf_webview2_packages_dir}")

    set(alwf_webview2_package_dir
        "${alwf_webview2_packages_dir}/Microsoft.Web.WebView2.${ALWF_WEBVIEW2_VERSION}")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
            set(alwf_webview2_arch "arm64")
        else()
            set(alwf_webview2_arch "x64")
        endif()
    else()
        set(alwf_webview2_arch "x86")
    endif()

    set(ALWF_WEBVIEW2_INCLUDE_DIR "${alwf_webview2_package_dir}/build/native/include")
    set(ALWF_WEBVIEW2_LOADER_DIR "${alwf_webview2_package_dir}/build/native/${alwf_webview2_arch}")
    set(ALWF_WEBVIEW2_LOADER_DLL "${ALWF_WEBVIEW2_LOADER_DIR}/WebView2Loader.dll")
    set(ALWF_WEBVIEW2_LOADER_LIB "${ALWF_WEBVIEW2_LOADER_DIR}/WebView2LoaderStatic.lib")

    foreach(required_path IN ITEMS
        "${ALWF_WEBVIEW2_INCLUDE_DIR}/WebView2.h"
        "${ALWF_WEBVIEW2_LOADER_DLL}"
        "${ALWF_WEBVIEW2_LOADER_LIB}")
        if(NOT EXISTS "${required_path}")
            message(FATAL_ERROR "Required WebView2 file was not found: ${required_path}")
        endif()
    endforeach()

    acbt_get_windows_dll_file_version("${ALWF_WEBVIEW2_LOADER_DLL}" WEBVIEW2_LOADER_VERSION)
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

    if(UNIX)
        set(LD_PREFIX LD_LIBRARY_PATH=${APP_LIB_DIR})
    endif()

    add_custom_command(
        OUTPUT "${OUT_HEADER}"
        COMMAND ${LD_PREFIX} $<TARGET_FILE:ahtt>
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

normalize_variable_name(PROJECT_NAME SRC_PREFIX)
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
    if(${CMAKE_BUILD_TYPE} MATCHES Release AND DEFINED ALWF_ICON)
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
if(NOT DEFINED ALWF_FRONTEND_DISABLE)
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
endif()

if(DEFINED ALWF_PUBLIC_DIR)
    if(NOT DEFINED ALWF_PUBLIC_UMLIB_OUTPUT)
        set(ALWF_PUBLIC_UMLIB_OUTPUT "${CMAKE_SOURCE_DIR}/${ALWF_STATIC_ASSETS_NAME}")
    endif()

    find_program(ALWF_UMBF_CONVERT_EXECUTABLE
        NAMES umbf-convert umbf-convert.exe
    )

    if(NOT ALWF_UMBF_CONVERT_EXECUTABLE AND DEFINED UMBF_CONVERT_TOOL_PATH AND NOT "${UMBF_CONVERT_TOOL_PATH}" STREQUAL "")
        set(ALWF_UMBF_CONVERT_EXECUTABLE "${UMBF_CONVERT_TOOL_PATH}")
    endif()

    if(NOT ALWF_UMBF_CONVERT_EXECUTABLE)
        message(FATAL_ERROR
            "umbf-convert was not found in PATH and UMBF_CONVERT_TOOL_PATH is empty. "
            "Set UMBF_CONVERT_TOOL_PATH to the converter executable path."
        )
    endif()

    if(NOT EXISTS "${ALWF_UMBF_CONVERT_EXECUTABLE}")
        message(FATAL_ERROR "umbf-convert executable does not exist: ${ALWF_UMBF_CONVERT_EXECUTABLE}")
    endif()

    file(GLOB_RECURSE ALWF_PUBLIC_FILES "${ALWF_PUBLIC_DIR}/*")

    if(UNIX)
        set(ALWF_PACK_LD_PREFIX LD_LIBRARY_PATH=${APP_LIB_DIR})
    endif()

    set(ALWF_PUBLIC_PACK_DEPS ${ALWF_PUBLIC_FILES} "${ALWF_UMBF_CONVERT_EXECUTABLE}")
    if(TARGET copy_alwf_js)
        list(APPEND ALWF_PUBLIC_PACK_DEPS copy_alwf_js)
    endif()

    add_custom_command(
        OUTPUT "${ALWF_PUBLIC_UMLIB_OUTPUT}"
        COMMAND ${ALWF_PACK_LD_PREFIX} "${ALWF_UMBF_CONVERT_EXECUTABLE}" convert -R -i "${ALWF_PUBLIC_DIR}" -o "${ALWF_PUBLIC_UMLIB_OUTPUT}" --format=raw --mapped --compressed
        DEPENDS ${ALWF_PUBLIC_PACK_DEPS}
        COMMENT "Pack ${ALWF_PUBLIC_DIR} -> ${ALWF_PUBLIC_UMLIB_OUTPUT}"
        VERBATIM
    )
    add_custom_target(pack_public_umlib DEPENDS "${ALWF_PUBLIC_UMLIB_OUTPUT}")
    list(APPEND DEPENDENT_TARGETS pack_public_umlib)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE ${ALWF_ROOT_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE acul umbf)
target_compile_definitions(${PROJECT_NAME} PRIVATE ALWF_STATIC_ASSETS_NAME="${ALWF_STATIC_ASSETS_NAME}")

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi awin)
    target_include_directories(${PROJECT_NAME} PRIVATE "${ALWF_WEBVIEW2_INCLUDE_DIR}")
    target_link_directories(${PROJECT_NAME} PRIVATE "${ALWF_WEBVIEW2_LOADER_DIR}")
    target_link_libraries(${PROJECT_NAME} PRIVATE WebView2LoaderStatic)
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
    acbt_get_target_output_dir("${PROJECT_NAME}" alwf_target_output_dir)
    set(alwf_webview2_output_path "${alwf_target_output_dir}/WebView2Loader.dll")
    normalize_variable_name("${PROJECT_NAME}_webview2_loader_copy" alwf_webview2_copy_target_name)

    add_custom_command(
        OUTPUT "${alwf_webview2_output_path}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${alwf_target_output_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ALWF_WEBVIEW2_LOADER_DLL}"
            "${alwf_webview2_output_path}"
        DEPENDS
            "${ALWF_WEBVIEW2_LOADER_DLL}"
        COMMENT "Copying WebView2Loader.dll for ${PROJECT_NAME}"
        VERBATIM
    )
    add_custom_target(${alwf_webview2_copy_target_name} ALL
        DEPENDS "${alwf_webview2_output_path}")
    list(APPEND DEPENDENT_TARGETS ${alwf_webview2_copy_target_name})

    # Manifest
    set(APP_MANIFEST_DEPS
        libacul:${ACUL_VERSION}
        libawin:${AWIN_VERSION}
        WebView2Loader:${WEBVIEW2_LOADER_VERSION}
        ${ALWF_MANIFEST_DEPS}
    )
    gen_manifest_lib(WebView2Loader ${WEBVIEW2_LOADER_VERSION})
    gen_manifest_app(${PROJECT_NAME} ${PROJECT_VERSION} ${APP_MANIFEST_DEPS})
    gen_app_config()
endif()

if(DEFINED DEPENDENT_TARGETS)
    add_dependencies(${PROJECT_NAME} ${DEPENDENT_TARGETS})
endif()
