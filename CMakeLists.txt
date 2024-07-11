cmake_minimum_required(VERSION 3.20...3.29.3)

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
include(cmake/common.cmake)
include(cmake/deps.cmake)

# set(THREADS_PREFER_PTHREAD_FLAG ON)
# find_package(Threads REQUIRED)
# target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

# set(CMAKE_EXE_LINKER_FLAGS "-fPIE")
target_sources(${PROJECT_NAME} PRIVATE ${DEPS_SOURCES})
target_compile_options(${PROJECT_NAME} PRIVATE -Wno-format-security -Wno-switch)

