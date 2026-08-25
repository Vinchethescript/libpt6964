cmake_minimum_required(VERSION 3.15)
project(pt6964 LANGUAGES CXX)

add_library(pt6964 INTERFACE)
add_library(pt6964::pt6964 ALIAS pt6964)

if(EXISTS "/proc/cpuinfo")
    file(READ "/proc/cpuinfo" CPU_INFO)
    if(CPU_INFO MATCHES "Raspberry Pi" OR CPU_INFO MATCHES "BCM2835|BCM2836|BCM2837|BCM2711|BCM2712")
        set(TARGET_RPI TRUE)
    endif()
endif()

option(TARGET_RPI "Enable PigpioInterface" ${TARGET_RPI})

if(TARGET_RPI)
    target_compile_definitions(pt6964 INTERFACE TARGET_RPI)
endif()

target_compile_features(pt6964 INTERFACE cxx_std_17)

target_include_directories(pt6964 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)