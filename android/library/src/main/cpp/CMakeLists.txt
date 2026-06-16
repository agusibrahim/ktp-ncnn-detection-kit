cmake_minimum_required(VERSION 3.18)
project(ktp_ncnn_library CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NCNN_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../ncnn-android")
set(NCNN_ABI_DIR "${NCNN_ROOT}/${ANDROID_ABI}")

include_directories(
    "${NCNN_ABI_DIR}/include"
    "${NCNN_ABI_DIR}/include/ncnn"
)

add_library(ktp_ncnn SHARED ktp_ncnn_jni.cpp)

target_link_libraries(ktp_ncnn
    "${NCNN_ABI_DIR}/lib/libncnn.a"
    "${NCNN_ABI_DIR}/lib/libglslang.a"
    "${NCNN_ABI_DIR}/lib/libSPIRV.a"
    "${NCNN_ABI_DIR}/lib/libMachineIndependent.a"
    "${NCNN_ABI_DIR}/lib/libOSDependent.a"
    "${NCNN_ABI_DIR}/lib/libGenericCodeGen.a"
    "${NCNN_ABI_DIR}/lib/libglslang-default-resource-limits.a"
    vulkan
    omp
    android
    log
)

