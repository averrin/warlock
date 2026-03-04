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
  GIT_TAG v3.5.0
)
# Patch sol2 v3.5.0 bug: iter::end() was renamed to iter::sen() but call sites weren't updated
if(sol2_ADDED)
  set(_sol2_container "${sol2_SOURCE_DIR}/include/sol/usertype_container.hpp")
  file(READ "${_sol2_container}" _sol2_container_src)
  string(REPLACE "auto& end = i.end();" "auto& end = i.sen();" _sol2_container_src "${_sol2_container_src}")
  file(WRITE "${_sol2_container}" "${_sol2_container_src}")
endif()
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${sol2_SOURCE_DIR}/include")
