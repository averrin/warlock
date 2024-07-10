include(cmake/CPM.cmake)
include(FetchContent)

set(FMT_VERSION 11.0.0)
set(LUA_VERSION 5.4.6)
set(TASKFLOW_VERSION 3.7.0)
set(ENTT_VERSION 3.13.2)
include(cmake/deps/imgui_sfml.cmake)
include(cmake/deps/lua.cmake)


target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${PROJECT_PATH}/include/3rdparty")

CPMAddPackage("gh:fmtlib/fmt#${FMT_VERSION}")
CPMAddPackage("gh:averrin/libcolor#master")
CPMAddPackage("gh:averrin/libprint#master")
CPMAddPackage("gh:averrin/liblog#master")

if(libcolor_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${libcolor_SOURCE_DIR}/include")
endif()
if(libprint_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${libprint_SOURCE_DIR}/include")
endif()
if(liblog_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${liblog_SOURCE_DIR}/include")
endif()

target_link_libraries(${EXE_NAME} PRIVATE
  fmt
)

FetchContent_Declare(backward
    GIT_REPOSITORY https://github.com/bombela/backward-cpp
    GIT_TAG master  # or a version tag, such as v1.6
    SYSTEM          # optional, the Backward include directory will be treated as system directory
)
FetchContent_MakeAvailable(backward)
target_link_libraries(${EXE_NAME} PUBLIC Backward::Interface)

# Other dependencies
CPMAddPackage("gh:juliettef/IconFontCppHeaders#main")
CPMAddPackage("gh:USCiLab/cereal#master")

if(IconFontCppHeaders_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${IconFontCppHeaders_SOURCE_DIR}")
endif()
if(cereal_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${cereal_SOURCE_DIR}/include")
endif()

FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
)
FetchContent_MakeAvailable(argparse)
target_link_libraries(${EXE_NAME} PRIVATE
  argparse
)

# Taskflow
FetchContent_Declare(
  taskflow
  URL "https://github.com/taskflow/taskflow/archive/refs/tags/v${TASKFLOW_VERSION}.zip"
)
set(TF_BUILD_EXAMPLES OFF)
set(TF_BUILD_TESTS OFF)
FetchContent_MakeAvailable(taskflow)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${taskflow_SOURCE_DIR}")

# ENTT

FetchContent_Declare(
  entt
  URL "https://github.com/skypjack/entt/releases/download/v${ENTT_VERSION}/entt-v${ENTT_VERSION}.tar.gz"
)
FetchContent_MakeAvailable(entt)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${entt_SOURCE_DIR}/single_include")
