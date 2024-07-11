#ifndef __IMGUISTL_H_
#define __IMGUISTL_H_
#include <imgui.h>
#include <string>
#include <vector>

using namespace ImGui;

namespace ImGui
{

bool ListEdit(const char* label, std::vector<std::string>& values);
bool Combo(const char* label, int* currIndex, std::vector<std::string>& values);
bool ListBox(const char* label, int* currIndex, std::vector<std::string>& values);
bool InputText(const char* label, std::string& str, size_t maxInputSize = 255,
    ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = 0, void* user_data = 0);

bool BufferingBar(const char* label, float value,  const ImVec2& size_arg, const ImU32& bg_col, const ImU32& fg_col);

bool Spinner(const char* label, float radius, int thickness, const ImU32& color);
} // end of namespace ImGui

#endif // __IMGUI-STL_H_
