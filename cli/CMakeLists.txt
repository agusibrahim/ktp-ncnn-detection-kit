cmake_minimum_required(VERSION 3.18)
project(ktp_ncnn_cli CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE AND EXISTS "/opt/homebrew/opt/libomp")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/libomp")
endif()

find_package(OpenMP REQUIRED)
find_package(ncnn REQUIRED)

add_executable(ktp_detect src/ktp_detect.cpp)
target_include_directories(ktp_detect PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(ktp_detect PRIVATE ncnn OpenMP::OpenMP_CXX)
