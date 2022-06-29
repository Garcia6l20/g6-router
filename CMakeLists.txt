cmake_minimum_required(VERSION 3.16)
project(g6-router
  LANGUAGES CXX
  VERSION 0.0.1
  )

if (NOT G6_CMAKE)
  include(FetchContent)
  FetchContent_declare(g6-cmake-fetch
    GIT_REPOSITORY https://github.com/Garcia6L20/g6-cmake.git
    GIT_TAG main)
  FetchContent_makeAvailable(g6-cmake-fetch)
endif()

find_package(ctre REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)

set_target_properties(${PROJECT_NAME} PROPERTIES
  EXPORT_NAME router)

target_link_libraries(${PROJECT_NAME} INTERFACE ctre::ctre)
target_include_directories(${PROJECT_NAME} INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
add_library(g6::router ALIAS ${PROJECT_NAME})

enable_testing()
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(DIRECTORY include/ DESTINATION include
  FILES_MATCHING PATTERN *)
