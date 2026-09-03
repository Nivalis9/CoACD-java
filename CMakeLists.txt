cmake_minimum_required(VERSION 3.24)
project(CoACDJava LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(COACD_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../CoACD-1.0.14"
    CACHE PATH "Path to the CoACD source tree")
if(NOT EXISTS "${COACD_SOURCE_DIR}/public/coacd.h")
    message(FATAL_ERROR "COACD_SOURCE_DIR is not a CoACD source tree: ${COACD_SOURCE_DIR}")
endif()

find_package(JNI REQUIRED)
if(NOT TARGET coacd)
    add_subdirectory("${COACD_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/coacd" EXCLUDE_FROM_ALL)
endif()

add_library(coacd_jni SHARED native/coacd_jni.cpp)
target_include_directories(coacd_jni PRIVATE "${COACD_SOURCE_DIR}/public")
target_link_libraries(coacd_jni PRIVATE JNI::JNI coacd)
target_compile_features(coacd_jni PRIVATE cxx_std_20)
set_target_properties(coacd_jni PROPERTIES OUTPUT_NAME "coacd_jni")

install(TARGETS coacd_jni LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
