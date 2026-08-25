cmake_minimum_required(VERSION 3.15)
project(pt6964 LANGUAGES CXX)

add_library(pt6964 INTERFACE)
add_library(pt6964::pt6964 ALIAS pt6964)

target_include_directories(pt6964 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)