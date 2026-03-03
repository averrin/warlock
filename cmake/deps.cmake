include(cmake/CPM.cmake)
include(FetchContent)

set(FMT_VERSION 11.0.0)
set(LUA_VERSION 5.4.6)
set(TASKFLOW_VERSION 3.7.0)
set(ENTT_VERSION 3.13.2)
include(cmake/deps/imgui_sfml.cmake)
include(cmake/deps/lua.cmake)


target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${PROJECT_PATH}/include/3rdparty")

IF (NOT CPM_SKIP_UPDATE)
CPMAddPackage("gh:fmtlib/fmt#${FMT_VERSION}")
CPMAddPackage("gh:averrin/libcolor#master")
CPMAddPackage("gh:averrin/libprint#master")
CPMAddPackage("gh:averrin/liblog#master")
# CPMAddPackage("gh:bobluppes/graaf#main")
CPMAddPackage("gh:mobius3/tweeny#master")

CPMAddPackage("gh:nlohmann/json@3.11.3")
CPMAddPackage("gh:ilqvya/random@1.5.0")
CPMAddPackage(
  NAME strutil
  DOWNLOAD_ONLY YES
  GITHUB_REPOSITORY tgalaj/strutil
    GIT_TAG v1.1.0
)

CPMAddPackage(
    NAME magic_enum
    GITHUB_REPOSITORY Neargye/magic_enum
    GIT_TAG v0.9.6
)
ENDIF()

target_link_libraries(${EXE_NAME} PRIVATE nlohmann_json::nlohmann_json)
if(libcolor_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${libcolor_SOURCE_DIR}/include")
endif()
if(libprint_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${libprint_SOURCE_DIR}/include")
endif()
if(liblog_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${liblog_SOURCE_DIR}/include")
endif()
if(random_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${random_SOURCE_DIR}/include")
endif()
if(strutil_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${strutil_SOURCE_DIR}")
endif()

if(magic_enum_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${magic_enum_SOURCE_DIR}/include/magic_enum")
endif()

if(graaf_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${graaf_SOURCE_DIR}/include")
endif()

if(tweeny_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${tweeny_SOURCE_DIR}/include")
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
IF (NOT CPM_SKIP_UPDATE)
CPMAddPackage("gh:juliettef/IconFontCppHeaders#main")
CPMAddPackage("gh:USCiLab/cereal#v1.3.2")
CPMAddPackage("gh:webview/webview#master")
ENDIF()

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
#FetchContent_Declare(
#  taskflow
#  URL "https://github.com/taskflow/taskflow/archive/refs/tags/v${TASKFLOW_VERSION}.zip"
#)
#set(TF_BUILD_EXAMPLES OFF)
#set(TF_BUILD_TESTS OFF)
#FetchContent_MakeAvailable(taskflow)
#target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${taskflow_SOURCE_DIR}")

# ENTT

FetchContent_Declare(
  entt
  URL "https://github.com/skypjack/entt/releases/download/v${ENTT_VERSION}/entt-v${ENTT_VERSION}.tar.gz"
)
FetchContent_MakeAvailable(entt)
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${entt_SOURCE_DIR}/single_include")
