cmake_minimum_required(VERSION 3.20)

include(FetchContent)
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# ─── Unit tests (no game engine) ─────────────────────────────────────────────
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

# ─── Serialization tests (FieldArchive, Store) ────────────────────────────────
add_executable(warlock_serial
  unit/test_field_archive.cpp
  unit/test_store.cpp
)
target_link_libraries(warlock_serial PRIVATE
  Catch2::Catch2WithMain
  fmt
)
target_include_directories(warlock_serial PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${entt_SOURCE_DIR}/single_include
  ${cereal_SOURCE_DIR}/include
  ${liblog_SOURCE_DIR}/include
  ${libprint_SOURCE_DIR}/include
  ${libcolor_SOURCE_DIR}/include
)
target_compile_features(warlock_serial PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(warlock_serial PRIVATE /W3 /wd4244 /wd4267 /wd4996 /wd4305 /Zc:__cplusplus /bigobj)
  target_compile_definitions(warlock_serial PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# ─── E2E tests (full headless game engine + WebSocket) ────────────────────────
file(GLOB_RECURSE E2E_GAME_SRC
  ${PROJECT_SOURCE_DIR}/src/game/*.cpp
  ${PROJECT_SOURCE_DIR}/src/rpc/*.cpp
  ${PROJECT_SOURCE_DIR}/src/utils/*.cpp
)
file(GLOB E2E_TEST_SRC
  ${CMAKE_CURRENT_SOURCE_DIR}/e2e/*.cpp
)

add_executable(warlock_e2e
  e2e_main.cpp
  harness.cpp
  rpc_client.cpp
  ${E2E_TEST_SRC}
  ${E2E_GAME_SRC}
  ${DEPS_SOURCES}           # Lua, etc. from parent CMake
)

target_include_directories(warlock_e2e PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}           # so e2e/*.cpp can find harness.hpp / rpc_client.hpp
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/3rdparty
  ${entt_SOURCE_DIR}/single_include
)
# Propagate all system includes from parent deps
get_target_property(_main_incs ${PROJECT_NAME} INCLUDE_DIRECTORIES)
if(_main_incs)
  target_include_directories(warlock_e2e SYSTEM PRIVATE ${_main_incs})
endif()
get_target_property(_main_sys_incs ${PROJECT_NAME} INTERFACE_INCLUDE_DIRECTORIES)
if(_main_sys_incs)
  target_include_directories(warlock_e2e SYSTEM PRIVATE ${_main_sys_incs})
endif()

target_link_libraries(warlock_e2e PRIVATE
  Catch2::Catch2
  nlohmann_json::nlohmann_json
  ixwebsocket
  fmt
  Backward::Interface
  argparse
)
if(MSVC)
  target_link_libraries(warlock_e2e PRIVATE ws2_32 winmm)
endif()

target_compile_features(warlock_e2e PUBLIC cxx_std_20)
target_compile_definitions(warlock_e2e PRIVATE
  _USE_MATH_DEFINES
  WARLOCK_PROJECT_ROOT="${PROJECT_SOURCE_DIR}"
  NDEBUG
)
if(MSVC)
  target_compile_options(warlock_e2e PRIVATE /W3 /wd4244 /wd4267 /wd4996 /wd4305 /Zc:__cplusplus /bigobj /FS)
  target_compile_definitions(warlock_e2e PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# CTest
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(warlock_unit)
catch_discover_tests(warlock_serial)
# catch_discover_tests(warlock_e2e)  # uncomment after first successful build
