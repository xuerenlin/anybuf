cmake_minimum_required(VERSION 3.0.0)
project(testcode VERSION 0.1.0)

### this project library name
set(CuTestLIB "cutest")

### options
set(CMAKE_C_FLAGS "${CAMKE_C_FLAGS} -g")

### include path
include_directories(
    "."
)

### this lib
add_library(${CuTestLIB} "")
target_sources(${CuTestLIB}
    PRIVATE
    "CuTest.c"
)
