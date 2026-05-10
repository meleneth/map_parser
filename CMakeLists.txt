cmake_minimum_required(VERSION 3.20)
project(fallout_map_parser CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CPM_DOWNLOAD_VERSION 0.40.8)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
  file(DOWNLOAD
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
    "${CPM_DOWNLOAD_LOCATION}"
    EXPECTED_HASH SHA256=78ba32abdf798bc616bab7c73aac32a17bbd7b06ad9e26a6add69de8f3ae47991a
  )
endif()

include("${CPM_DOWNLOAD_LOCATION}")

CPMAddPackage(
  NAME Catch2
  GITHUB_REPOSITORY catchorg/Catch2
  VERSION 3.7.1
)

add_library(map_parser INTERFACE)
target_include_directories(map_parser INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_executable(test_map_parser
  test/test_map_parser.cpp
)

target_link_libraries(test_map_parser
  PRIVATE
    map_parser
    Catch2::Catch2WithMain
)

include(CTest)

if(BUILD_TESTING)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  include(Catch)
  catch_discover_tests(test_map_parser)
endif()
