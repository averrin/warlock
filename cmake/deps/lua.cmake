FetchContent_Declare(
  lua
  URL "https://github.com/lua/lua/archive/refs/tags/v${LUA_VERSION}.tar.gz"
  )
FetchContent_MakeAvailable(lua)
file(GLOB LUA_SOURCE "${lua_SOURCE_DIR}/*.c")
# Remove CLI and combined sources
list(REMOVE_ITEM LUA_SOURCE "${lua_SOURCE_DIR}/lua.c")
list(REMOVE_ITEM LUA_SOURCE "${lua_SOURCE_DIR}/luac.c")
list(REMOVE_ITEM LUA_SOURCE "${lua_SOURCE_DIR}/onelua.c")

list(APPEND DEPS_SOURCES ${LUA_SOURCE})
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${lua_SOURCE_DIR}")

target_link_libraries(${EXE_NAME} PRIVATE
  # lua
)

CPMAddPackage(
  NAME sol2
  GITHUB_REPOSITORY ThePhD/sol2
  GIT_TAG v3.3.0
)
file(READ "${sol2_SOURCE_DIR}/include/sol/optional_implementation.hpp" _sol2_optional_hpp)
if(_sol2_optional_hpp MATCHES "T& emplace\\(Args&&\\.\\.\\. args\\) noexcept")
  # Patch sol2 for GCC 15 compatibility using cmake string replacement.
  # This avoids any dependency on git or patch utilities (CPM may download
  # sol2 as a zip archive with no .git directory).
  string(REPLACE
    "\t\ttemplate <class... Args>\n\t\tT& emplace(Args&&... args) noexcept {\n\t\t\tstatic_assert(std::is_constructible<T, Args&&...>::value, \"T must be constructible with Args\");\n\n\t\t\t*this = nullopt;\n\t\t\tthis->construct(std::forward<Args>(args)...);\n\t\t}"
    "\t\tT& emplace(T& arg) noexcept {\n\t\t\t*this = nullopt;\n\t\t\tm_value = std::addressof(arg);\n\t\t\treturn **this;\n\t\t}"
    _sol2_optional_patched "${_sol2_optional_hpp}"
  )
  if(_sol2_optional_patched STREQUAL _sol2_optional_hpp)
    message(FATAL_ERROR "sol2: GCC15 patch string not found in optional_implementation.hpp")
  endif()
  file(WRITE "${sol2_SOURCE_DIR}/include/sol/optional_implementation.hpp" "${_sol2_optional_patched}")
  message(STATUS "sol2: applied GCC 15 optional_implementation.hpp patch")
endif()
target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC "${sol2_SOURCE_DIR}/include")
