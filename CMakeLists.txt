cmake_minimum_required(VERSION 3.15)
project(pt6964 LANGUAGES CXX)

add_library(pt6964 INTERFACE)
add_library(pt6964::pt6964 ALIAS pt6964)

option(ENBALE_PIGPIO "Enable Raspberry Pi-specific interfaces" OFF)

if(ENABLE_PIGPIO)
    target_compile_definitions(pt6964 INTERFACE ENABLE_PIGPIO)
endif()

target_compile_features(pt6964 INTERFACE cxx_std_17)

target_include_directories(pt6964 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)