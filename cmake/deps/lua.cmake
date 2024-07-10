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
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${sol2_SOURCE_DIR}/include")
