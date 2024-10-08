
set(SFML_VERSION 2.6.1)
# set(IMGUI_VERSION 1.90.6)
FetchContent_Declare(
  imgui
  # URL "https://github.com/ocornut/imgui/archive/v${IMGUI_VERSION}.zip"
  URL "https://github.com/ocornut/imgui/archive/refs/heads/docking.zip"
)
FetchContent_MakeAvailable(imgui)

CPMAddPackage("gh:epezent/implot#master")

if(implot_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${implot_SOURCE_DIR}")

  file(GLOB IMPLOT_SOURCE "${implot_SOURCE_DIR}/*.cpp")
  list(APPEND DEPS_SOURCES ${IMPLOT_SOURCE})
endif()


FetchContent_Declare(
  SFML
  URL "https://github.com/SFML/SFML/archive/${SFML_VERSION}.zip"
)

option(SFML_BUILD_AUDIO "Build audio" OFF)
option(SFML_BUILD_NETWORK "Build network" OFF)
set(OpenGL_GL_PREFERENCE "GLVND")
# set(OpenGL_GL_PREFERENCE "LEGACY")
find_package(OpenGL REQUIRED)
FetchContent_MakeAvailable(sfml)

FetchContent_Declare(
  imgui-sfml
  GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
  GIT_TAG        2.6.x
)

set(IMGUI_DIR ${imgui_SOURCE_DIR})
option(IMGUI_SFML_FIND_SFML "Use find_package to find SFML" OFF)
option(IMGUI_SFML_IMGUI_DEMO "Build imgui_demo.cpp" ON)
FetchContent_MakeAvailable(imgui-sfml)

find_package(OpenGL REQUIRED)

file(GLOB IMGUI_SOURCE "${IMGUI_DIR}/*.cpp")
file(GLOB IMGUI_SOURCE_STDLIB "include/3rdparty/imgui-stl.cpp")
list(APPEND DEPS_SOURCES ${IMGUI_SOURCE_STDLIB})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC ${OPENGL_INCLUDE_DIR})

CPMAddPackage("gh:santaclose/ImGuiColorTextEdit#master")

if(ImGuiColorTextEdit_ADDED)
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${ImGuiColorTextEdit_SOURCE_DIR}")
  target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${ImGuiColorTextEdit_SOURCE_DIR}/vendor/regex/include")
  file(GLOB IMGUICTE_SOURCE "${ImGuiColorTextEdit_SOURCE_DIR}/*.cpp")
  file(GLOB RE_SOURCE "${ImGuiColorTextEdit_SOURCE_DIR}/vendor/regex/src/*.cpp")
  list(APPEND DEPS_SOURCES ${IMGUICTE_SOURCE})
  list(APPEND DEPS_SOURCES ${RE_SOURCE})
endif()

target_link_libraries(${EXE_NAME} PRIVATE
  ImGui-SFML::ImGui-SFML
  OpenGL
  #stdc++fs
  sfml-system sfml-window sfml-graphics
)
