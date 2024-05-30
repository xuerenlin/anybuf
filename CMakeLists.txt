cmake_minimum_required(VERSION 3.5.0)
project(testcode VERSION 0.1.0)

### this project library name
set(THISLIB "anybuf")
set(THISLIB_STATIC "anybuf_a")
set(THISLIB_SHARED "anybuf_so")

### options
option(NEED_BUILD_TESTS "Build unit tests" ON)
set(CMAKE_CXX_FLAGS "${CAMKE_CXX_FLAGS} -O2 -std=c++11 -pthread")
set(CMAKE_C_FLAGS "${CAMKE_C_FLAGS} -O2 -pthread")

### include path
include_directories(
    "abuf"
)

### test
if(NEED_BUILD_TESTS)
    #include(CTest)
    enable_testing()
    add_subdirectory("third/cutest")
    include_directories("third/cutest")

    function(sub_test test_file)
        get_filename_component(test_target_name "${test_file}" NAME_WE)

        add_executable("${test_target_name}" "")
        target_sources("${test_target_name}"
            PRIVATE
            "${test_file}"
    )
    target_link_libraries("${test_target_name}" ${THISLIB_SHARED} cutest)
    add_test(NAME "${test_target_name}" COMMAND "${test_target_name}")
  endfunction(sub_test)
  
  sub_test("test/test_ab_jsonbin.c")
  sub_test("test/test_ab_jsonpath.c")
  sub_test("test/test_ab_mem.c")
  sub_test("test/test_ab_list.c")
  sub_test("test/test_ab_skiplist.c")
  sub_test("test/test_ab_hash.c")
  sub_test("test/test_ab_buf.c")
  sub_test("test/test_ab_perf_diff.cc")
endif(NEED_BUILD_TESTS)

### this lib
SET(LIBSRC 
    "abuf/ab_os.c"
    "abuf/ab_error.c"
    "abuf/ab_mem.c"
    "abuf/ab_list.c"
    "abuf/ab_skiplist.c"
    "abuf/ab_hash.c"
    "abuf/ab_buf.c"
    "abuf/ab_jsonbin.c"
    "abuf/ab_jsonpath.c"
)
add_library(${THISLIB_STATIC} STATIC ${LIBSRC})
SET_TARGET_PROPERTIES(${THISLIB_STATIC} PROPERTIES OUTPUT_NAME "${THISLIB}")

add_library(${THISLIB_SHARED} SHARED ${LIBSRC})
SET_TARGET_PROPERTIES(${THISLIB_SHARED} PROPERTIES OUTPUT_NAME "${THISLIB}")


ADD_CUSTOM_COMMAND(TARGET ${THISLIB_STATIC} POST_BUILD
  COMMAND echo "Create All IN One File: anybuf.c anybuf.h"
  COMMAND ../onefile.sh
  COMMAND gcc ../anybuf.c ../main.c -o anybuf
  VERBATIM
  )
