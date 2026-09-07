cmake_minimum_required(VERSION 3.20)
project(UtilizationObservatory VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(UO_BUILD_TESTS "Build Utilization Observatory tests" ON)
option(UO_BUILD_CLI "Build the uo CLI" ON)
option(UO_BUILD_EXAMPLES "Build examples" ON)
option(UO_BUILD_BENCHMARKS "Build benchmarks" ON)
option(UO_ENABLE_CUDA "Build the CUDA/NVML hardware proof backend (requires CUDA)" OFF)
option(UO_BUILD_DISTRIBUTED "Build the distributed TCP proof" ON)
option(UO_ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(UO_ENABLE_ASAN AND MSVC)
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/fsanitize=address)
endif()

add_library(utilization_observatory
    src/observatory.cpp
    src/model.cpp
    src/time.cpp
    src/codec.cpp
)
add_library(UtilizationObservatory::UtilizationObservatory ALIAS utilization_observatory)
set_target_properties(utilization_observatory PROPERTIES EXPORT_NAME UtilizationObservatory)

target_include_directories(utilization_observatory PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(utilization_observatory PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(utilization_observatory PRIVATE -Wall -Wextra -Werror)
endif()

if(UO_ENABLE_CUDA AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/cuda/CMakeLists.txt")
    add_subdirectory(src/cuda)
endif()
if(UO_BUILD_CLI AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cli/CMakeLists.txt")
    add_subdirectory(cli)
endif()
if(UO_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()
if(UO_BUILD_DISTRIBUTED AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dist/CMakeLists.txt")
    add_subdirectory(dist)
endif()
if(UO_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests)
endif()
if(UO_BUILD_BENCHMARKS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/CMakeLists.txt")
    add_subdirectory(benchmarks)
endif()

include(GNUInstallDirs)
install(TARGETS utilization_observatory
    EXPORT UtilizationObservatoryTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/UtilizationObservatoryConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/UtilizationObservatoryConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/UtilizationObservatoryConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UtilizationObservatory
)
install(EXPORT UtilizationObservatoryTargets
    FILE UtilizationObservatoryTargets.cmake
    NAMESPACE UtilizationObservatory::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UtilizationObservatory
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/UtilizationObservatoryConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/UtilizationObservatoryConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/UtilizationObservatory
)
