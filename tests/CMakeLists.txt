cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# ─── Unit tests (no game engine, no SFML) ─────────────────────────────────────
add_executable(warlock_unit
  unit/test_message.cpp
  unit/test_router.cpp
  unit/test_graph.cpp
  unit/test_spatial_hash.cpp
  ${PROJECT_SOURCE_DIR}/src/rpc/message.cpp
  ${PROJECT_SOURCE_DIR}/src/rpc/router.cpp
  ${PROJECT_SOURCE_DIR}/src/utils/graph.cpp
)
target_link_libraries(warlock_unit PRIVATE
  Catch2::Catch2WithMain
  nlohmann_json::nlohmann_json
  fmt
)
target_include_directories(warlock_unit PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${entt_SOURCE_DIR}/single_include
)
target_compile_features(warlock_unit PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(warlock_unit PRIVATE /W3 /wd4244 /wd4267 /wd4996 /wd4305 /Zc:__cplusplus /bigobj)
  target_compile_definitions(warlock_unit PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(warlock_unit)
