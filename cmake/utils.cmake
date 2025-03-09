include(CheckCXXSourceCompiles)

# Check SIMD features

function(check_compiler_define DEFINE RESULT)
    CHECK_CXX_SOURCE_COMPILES("
    #include <limits.h>
    #ifndef ${DEFINE}
    #error \"${DEFINE} is not a macro!\"
    #endif
    int main() { return 0; }
    " ${RESULT})
    set(${RESULT} ${${RESULT}} PARENT_SCOPE)
endfunction()

if(NOT PREFIX_simd_DISABLE)
    check_compiler_define("__AVX__" SIMD_AVX)
    check_compiler_define("__AVX2__" SIMD_AVX2)
    check_compiler_define("__AVX512F__" SIMD_AVX512)
    check_compiler_define("__SSE4_2__" SIMD_SSE42)

    set(PREFIX_simd "avx512;SIMD_AVX512" "avx2;SIMD_AVX2" "avx;SIMD_AVX" "sse42;SIMD_SSE42")
    list(LENGTH PREFIX_simd list_length)
    math(EXPR max_index "${list_length} - 1")

    foreach(idx RANGE 0 ${max_index} 2)
        math(EXPR next_idx "${idx} + 1")
        list(GET PREFIX_simd ${idx} item1)
        list(GET PREFIX_simd ${next_idx} item2)

        if(NOT ${item2})
            continue()
        else()
            set(APP_SIMD_ARCH ${item1})
            break()
        endif()
    endforeach()

else()
    set(APP_SIMD_ARCH "nosimd")
endif(NOT PREFIX_simd_DISABLE)

set(PREFIX_os "win32;WIN32" "linux;LINUX" "osx;APPLE")

# Checks if a file exists and sets a variable to the result.
function(isFileExists file_list target_file result_var)
    list(FIND file_list "${target_file}" index)

    if(index GREATER_EQUAL 0)
        set(${result_var} TRUE PARENT_SCOPE)
    else()
        set(${result_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

# Checks if a given pair is present in a list and sets a variable to the result.
macro(list_contains_pair LIST_VAR VALUE RESULT_VAR)
    set(${RESULT_VAR} FALSE)

    if(DEFINED ${LIST_VAR})
        list(LENGTH ${LIST_VAR} list_length)

        if(list_length GREATER 1)
            math(EXPR max_index "${list_length} - 1")

            foreach(idx RANGE 0 ${max_index} 2)
                math(EXPR next_idx "${idx} + 1")
                list(GET ${LIST_VAR} ${idx} item1)
                list(GET ${LIST_VAR} ${next_idx} item2)

                if("${item1};${item2}" STREQUAL "${VALUE}")
                    set(${RESULT_VAR} TRUE)
                    break()
                endif()
            endforeach()
        endif()
    endif()
endmacro()

# Filters a list of files based on a given pattern and outputs the filtered list to another variable.
function(filter_files FILE_LIST OUT_LIST)
    foreach(file ${FILE_LIST})
        string(REGEX REPLACE "(.*)__([a-z]+)_[a-z0-9]+_(.*)" "\\1;\\2;\\3" PREFIXES ${file})
        list(LENGTH PREFIXES count)

        if(count GREATER 1)
            list(GET PREFIXES 0 ABS_PATH)
            list(GET PREFIXES 1 PREFIX)
            list(GET PREFIXES 2 FILE_BASENAME)
            set(FEATURE_VAR "PREFIX_${PREFIX}_FOUNDS")

            if(DEFINED ${FEATURE_VAR} AND NOT "${${FEATURE_VAR}}" STREQUAL "")
                set(FILE_PROCESS_CHECK "${ABS_PATH}${FILE_BASENAME}")
                list_contains_pair(${FEATURE_VAR} "${FILE_PROCESS_CHECK}" FILE_ALREADY_PROCESSED)

                if(FILE_ALREADY_PROCESSED)
                    continue()
                endif(FILE_ALREADY_PROCESSED)
            endif()

            set(PREFIX_LIST_VAR "PREFIX_${PREFIX}")

            if(DEFINED ${PREFIX_LIST_VAR})
                list(LENGTH "PREFIX_${PREFIX}" prefix_length)
                math(EXPR max_index "${prefix_length} - 1")

                foreach(idx RANGE 0 ${max_index} 2)
                    math(EXPR next_idx "${idx} + 1")
                    list(GET "PREFIX_${PREFIX}" ${idx} current_prefix)
                    list(GET "PREFIX_${PREFIX}" ${next_idx} current_feature)

                    if(NOT ${current_feature})
                        continue()
                    endif()

                    set(TO_FIND_NAME "${ABS_PATH}__${PREFIX}_${current_prefix}_${FILE_BASENAME}")
                    isFileExists("${FILE_LIST}" ${TO_FIND_NAME} FILE_FOUND)

                    if(FILE_FOUND)
                        list(APPEND PREFIX_${PREFIX}_FOUNDS "${ABS_PATH};${FILE_BASENAME}")
                        list(APPEND OUT_LIST ${TO_FIND_NAME})
                        break()
                    endif()
                endforeach()

                if(NOT DEFINED ${FEATURE_VAR})
                    message(SEND_ERROR "${PREFIX} features are not supported")
                endif()
            elseif(NOT DEFINED PREFIX_${PREFIX}_DISABLE)
                message(SEND_ERROR "Prefix does not match any pattern: ${PREFIX}")
            endif()
        else()
            list(APPEND OUT_LIST ${file})
        endif()
    endforeach()

    set(${OUT_LIST} ${${OUT_LIST}} PARENT_SCOPE)
endfunction()

# Adds files to a given target
function(add_files target_name)
    set(options RECURSE)
    set(oneValueArgs)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "${options}" "${oneValueArgs}" "${multiValueArgs}")

    set(APP_MODULES_LIST "")

    foreach(BASE_DIR ${ARG_UNPARSED_ARGUMENTS})
        list(APPEND APP_MODULES_LIST "${BASE_DIR}/*.cpp")

        if(APPLE)
            list(APPEND APP_MODULES_LIST "${BASE_DIR}/*.mm")
        endif()
    endforeach()

    foreach(APP_MODULE_PATH ${APP_MODULES_LIST})
        if(ARG_RECURSE)
            file(GLOB_RECURSE TEMP_APP_MODULE_SRC ${APP_MODULE_PATH})
        else()
            file(GLOB TEMP_APP_MODULE_SRC ${APP_MODULE_PATH})
        endif()

        filter_files("${TEMP_APP_MODULE_SRC}" FILTERED_FILES)
        list(APPEND SOURCE_FILES ${FILTERED_FILES})
    endforeach()

    set(SOURCE_FILES ${SOURCE_FILES} PARENT_SCOPE)
endfunction()

function(add_test_files TEST_LABEL TEST_NAME SOURCE_FILES INCLUDES LIBRARIES ENV_VAR)
    set(TEST_FINAL_NAME "${TEST_LABEL}_${TEST_NAME}")
    add_executable(${TEST_FINAL_NAME} ${SOURCE_FILES})
    target_include_directories(${TEST_FINAL_NAME} PRIVATE ${INCLUDES})
    add_test(NAME ${TEST_FINAL_NAME} COMMAND "${CMAKE_BINARY_DIR}/tests/${TEST_FINAL_NAME}" WORKING_DIRECTORY ${APP_LIB_DIR})
    target_link_libraries(${TEST_FINAL_NAME} PRIVATE ${LIBRARIES})
    set_tests_properties(${TEST_FINAL_NAME} PROPERTIES LABELS ${TEST_LABEL})
    set_tests_properties(${TEST_FINAL_NAME} PROPERTIES ENVIRONMENT "${ENV_VAR}")
    set_target_properties(${TEST_FINAL_NAME}
        PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS YES
    )
endfunction(add_test_files)

# Override project() to get project version
cmake_policy(SET CMP0048 NEW)

macro(project)
    _project(${ARGN})
    set(${PROJECT_NAME}_VERSION "${${PROJECT_NAME}_VERSION}" CACHE INTERNAL "")
endmacro()