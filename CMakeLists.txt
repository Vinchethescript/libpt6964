cmake_minimum_required(VERSION 3.15)
project(pt6964 LANGUAGES CXX VERSION 1.0.0)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ENABLE_PIGPIO "Enable Raspberry Pi-specific interfaces" OFF)

add_library(pt6964 INTERFACE)
add_library(pt6964::pt6964 ALIAS pt6964)

target_compile_features(pt6964 INTERFACE cxx_std_17)

target_include_directories(pt6964 INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(ENABLE_PIGPIO)
    find_package(pigpio REQUIRED)
    target_compile_definitions(pt6964 INTERFACE ENABLE_PIGPIO)
    target_link_libraries(pt6964 INTERFACE pigpio)
endif()

install(TARGETS pt6964
    EXPORT pt6964_targets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT pt6964_targets
    FILE pt6964_targets.cmake
    NAMESPACE pt6964::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pt6964
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/pt6964_config_version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/pt6964_config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pt6964_config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pt6964
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/pt6964_config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/pt6964_config_version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/pt6964
)