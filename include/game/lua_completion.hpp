#pragma once

#include <string>
#include <vector>

#include <sol/forward.hpp>

class CodeExecutionSystem;
struct Frame;

/** Sync frame + environment globals on the per-frame Lua state (same as core execution). */
void syncFrameLuaEnvironment(CodeExecutionSystem& exec, int frame_id, Frame* frame_ptr);

/** List keys for autocomplete: empty path = globals; otherwise dot-separated (e.g. frame.data). */
std::vector<std::string> luaCompletionKeys(sol::state_view lua, const std::string& path);
