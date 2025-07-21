cmake_minimum_required(VERSION 3.17)
project(webview-app
    VERSION 0.0.1
    LANGUAGES CXX C
    DESCRIPTION "WebView Sample App")

set(APP_NAME "webview-app")
include(cmake/utils.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(USE_ASAN "Use Address Sanitizer" OFF)
set(USE_ASAN OFF)

add_compile_options(
    "$<$<CONFIG:Debug>:-g;-O0;-Wall>"
    "$<$<CONFIG:Release>:-O3;-DNDEBUG;-fomit-frame-pointer>"
)

set(APP_LIB_DIR ${CMAKE_SOURCE_DIR}/lib/${CMAKE_SYSTEM_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${APP_LIB_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${APP_LIB_DIR})

if(WIN32)
    include(cmake/manifest.cmake)
endif()

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

set(ACUL_BUILD_MIN ON)
add_subdirectory("modules/acul")

if(WIN32)
    add_subdirectory("modules/awin")

    if(DEFINED ENV{WEBVIEW2_SDK_PATH})
        include_directories("$ENV{WEBVIEW2_SDK_PATH}/build/native/include")
    else()
        message(FATAL_ERROR "WEBVIEW2_SDK_PATH is not set in the environment.")
    endif()
endif()

add_files(${PROJECT_NAME} RECURSE "${CMAKE_CURRENT_SOURCE_DIR}/src/app")

set(PUG_TEMPLATES_DIR "${CMAKE_SOURCE_DIR}/views")
set(PUG_COMPILER_DIR "${CMAKE_SOURCE_DIR}/src/pug-compile")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/templates")

file(GLOB PUG_FILES "${PUG_TEMPLATES_DIR}/*.pug")
file(GLOB PUG_COMPILER_FILES "${PUG_COMPILER_DIR}/*" "${PUG_COMPILER_DIR}/templates/*")
set(GENERATED_SOURCES "")

foreach(PUG_FILE ${PUG_FILES})
    get_filename_component(PUG_NAME ${PUG_FILE} NAME_WE)
    set(GENERATED_SOURCE "${GENERATED_DIR}/${PUG_NAME}.cpp")
    add_custom_command(
        OUTPUT ${GENERATED_HEADER} ${GENERATED_SOURCE}
        COMMAND npm start -- ${PUG_FILE} ${GENERATED_DIR}
        WORKING_DIRECTORY ${PUG_COMPILER_DIR}
        DEPENDS ${PUG_FILE} ${PUG_COMPILER_FILES}
        COMMENT "Compiling template: ${PUG_FILE}"
    )
    list(APPEND GENERATED_SOURCES ${GENERATED_SOURCE} ${GENERATED_HEADER})
endforeach()

add_custom_target(generate_pug_templates
    DEPENDS ${GENERATED_SOURCES}
)

add_subdirectory(src/app/locales)
list(APPEND DEPENDENT_TARGETS LOCALES)

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${GENERATED_SOURCES})

if(WIN32 AND NOT CMAKE_BUILD_TYPE MATCHES Debug)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE ON)
endif()

if(WIN32)
    if(${CMAKE_BUILD_TYPE} MATCHES Release)
        set(RES_SRC ${CMAKE_BINARY_DIR}/resources/app.rc)
        set(APP_RESOURCE ${CMAKE_BINARY_DIR}/resources/app.res)
        set(ICON_PATH ${CMAKE_SOURCE_DIR}/assets/icon.ico)
        configure_file(${TEMPLATES_DIR}/app.rc.in ${RES_SRC})

        add_custom_command(
            OUTPUT ${APP_RESOURCE}
            COMMAND ${CMAKE_RC_COMPILER} ${RES_SRC} -O coff -o ${APP_RESOURCE}
            DEPENDS ${RES_SRC}
            COMMENT "Generate resource: ${APP_RESOURCE}")

        add_custom_target(WIN_RESOURCES DEPENDS ${APP_RESOURCE})
        list(APPEND DEPENDENT_TARGETS WIN_RESOURCES)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${APP_RESOURCE})
    endif()
endif(WIN32)

add_dependencies(${PROJECT_NAME} ${DEPENDENT_TARGETS})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
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

if(WIN32)
    # Manifest
    set(APP_MANIFEST_DEPS
        acul:${acul_VERSION}
        awin:${awin_VERSION}
        WebView2Loader:1.0.3065
    )

    gen_manifest_app(${PROJECT_NAME} ${PROJECT_VERSION} ${APP_MANIFEST_DEPS})
    gen_app_config()
endif()