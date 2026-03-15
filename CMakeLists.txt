cmake_minimum_required(VERSION 3.20...3.29.3)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

set(APP_NAME "Hellfrost")
project(hellfrost VERSION 0.1.0)

configure_file (
    "${PROJECT_SOURCE_DIR}/include/meta.hpp.in"
    "${PROJECT_SOURCE_DIR}/include/meta.hpp"
)

file(GLOB_RECURSE SOURCE "src/*.cpp")
add_executable(${PROJECT_NAME}
  ${SOURCE}
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

# Skip cereal's Boost-based performance tests
set(SKIP_PERFORMANCE_COMPARISON ON CACHE BOOL "" FORCE)
set(BUILD_DOC OFF CACHE BOOL "" FORCE)
set(BUILD_SANDBOX OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)

include(cmake/common.cmake)
include(cmake/deps.cmake)

target_sources(${PROJECT_NAME} PRIVATE ${DEPS_SOURCES})

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W3 /wd4244 /wd4267 /wd4996 /wd4305 /Zc:__cplusplus /bigobj)
  target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-security -Wno-switch)
endif()

