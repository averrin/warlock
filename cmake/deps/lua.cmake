FetchContent_Declare(
  lua
  URL "https://www.lua.org/ftp/lua-${LUA_VERSION}.tar.gz"
  )
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCE "${lua_SOURCE_DIR}/src/*.c")
list(REMOVE_ITEM LUA_SOURCE "${lua_SOURCE_DIR}/src/lua.c")
list(REMOVE_ITEM LUA_SOURCE "${lua_SOURCE_DIR}/src/luac.c")

list(APPEND DEPS_SOURCES ${LUA_SOURCE})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${lua_SOURCE_DIR}/src")

target_link_libraries(${EXE_NAME} PRIVATE
  # lua
)

CPMAddPackage(
  NAME sol2
  GITHUB_REPOSITORY ThePhD/sol2
  GIT_TAG v3.3.0
)
find_program(GIT_EXECUTABLE NAMES git REQUIRED)
set(_sol2_gcc15_patch "${PROJECT_SOURCE_DIR}/cmake/patches/sol2-gcc15-optional-tref.patch")
file(READ "${sol2_SOURCE_DIR}/include/sol/optional_implementation.hpp" _sol2_optional_hpp)
if(_sol2_optional_hpp MATCHES "T& emplace\\(Args&&\\.\\.\\. args\\) noexcept")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --ignore-whitespace "${_sol2_gcc15_patch}"
    WORKING_DIRECTORY "${sol2_SOURCE_DIR}"
    RESULT_VARIABLE _sol2_patch_rc
  )
  if(NOT _sol2_patch_rc EQUAL 0)
    message(FATAL_ERROR "sol2: applying ${_sol2_gcc15_patch} failed (exit ${_sol2_patch_rc})")
  endif()
endif()
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${sol2_SOURCE_DIR}/include")
