#include <imgui-stl.hpp>
#include <imgui.h>
#include <imgui_internal.h>

using namespace ImGui;
namespace ImGui {

// well, at least it is possible..
static auto vector_getter = [](void *vec, int idx, const char **out_text) {
  auto &vector = *reinterpret_cast<std::vector<std::string> *>(vec);
  if (idx < 0 || idx >= static_cast<int>(vector.size())) {
    return false;
  }
  *out_text = vector.at(idx).c_str();
  return true;
};

bool Spinner(const char *label, float radius, int thickness,
             const ImU32 &color) {
  ImGuiWindow *window = GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext &g = *GImGui;
  const ImGuiStyle &style = g.Style;
  const ImGuiID id = window->GetID(label);

  ImVec2 pos = window->DC.CursorPos;
  ImVec2 size((radius)*2, (radius + style.FramePadding.y) * 2);

  const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
  ItemSize(bb, style.FramePadding.y);
  if (!ItemAdd(bb, id))
    return false;

  // Render
  window->DrawList->PathClear();

  int num_segments = 30;
  int start = abs(ImSin(g.Time * 1.8f) * (num_segments - 5));

  const float a_min = IM_PI * 2.0f * ((float)start) / (float)num_segments;
  const float a_max =
      IM_PI * 2.0f * ((float)num_segments - 3) / (float)num_segments;

  const ImVec2 centre =
      ImVec2(pos.x + radius, pos.y + radius + style.FramePadding.y);

  for (int i = 0; i < num_segments; i++) {
    const float a = a_min + ((float)i / (float)num_segments) * (a_max - a_min);
    window->DrawList->PathLineTo(
        ImVec2(centre.x + ImCos(a + g.Time * 8) * radius,
               centre.y + ImSin(a + g.Time * 8) * radius));
  }

  window->DrawList->PathStroke(color, false, thickness);
  return false;
}
bool BufferingBar(const char *label, float value, const ImVec2 &size_arg,
                  const ImU32 &bg_col, const ImU32 &fg_col) {
  ImGuiWindow *window = GetCurrentWindow();
  if (window->SkipItems)
    return false;

  ImGuiContext &g = *GImGui;
  const ImGuiStyle &style = g.Style;
  const ImGuiID id = window->GetID(label);

  ImVec2 pos = window->DC.CursorPos;
  ImVec2 size = size_arg;
  size.x -= style.FramePadding.x;
  pos.y += style.FramePadding.y + size.y / 2;

  const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));
  ItemSize(bb, style.FramePadding.y);
  if (!ItemAdd(bb, id))
    return false;

  // Render
  const float circleStart = size.x * 0.7f;
  const float circleEnd = size.x;
  const float circleWidth = circleEnd - circleStart;

  window->DrawList->AddRectFilled(bb.Min, ImVec2(pos.x + circleStart, bb.Max.y),
                                  bg_col);
  window->DrawList->AddRectFilled(
      bb.Min, ImVec2(pos.x + circleStart * value, bb.Max.y), fg_col);

  const float t = g.Time;
  const float r = size.y / 2;
  const float speed = 1.5f;

  const float a = speed * 0;
  const float b = speed * 0.333f;
  const float c = speed * 0.666f;

  const float o1 =
      (circleWidth + r) * (t + a - speed * (int)((t + a) / speed)) / speed;
  const float o2 =
      (circleWidth + r) * (t + b - speed * (int)((t + b) / speed)) / speed;
  const float o3 =
      (circleWidth + r) * (t + c - speed * (int)((t + c) / speed)) / speed;

  window->DrawList->AddCircleFilled(
      ImVec2(pos.x + circleEnd - o1, bb.Min.y + r), r, bg_col);
  window->DrawList->AddCircleFilled(
      ImVec2(pos.x + circleEnd - o2, bb.Min.y + r), r, bg_col);
  window->DrawList->AddCircleFilled(
      ImVec2(pos.x + circleEnd - o3, bb.Min.y + r), r, bg_col);
  return false;
}

std::string newItem;
bool ListEdit(const char *label, std::vector<std::string> &values) {
  auto n = 0;
  if (!values.empty()) {
    std::vector<int> to_remove;
    Text("%s count: %lu", label, values.size());
    for (auto v : values) {
      char label[11];
      std::snprintf(label, sizeof(label), "##%d-input", n);
      InputText(label, v);
      SameLine();
      char blabel[13];
      std::snprintf(blabel, sizeof(blabel), "X##%d-button", n);
      if (Button(blabel)) {
        to_remove.push_back(n);
      }
      n++;
    }
    for (auto rk : to_remove) {
      values.erase(values.begin() + rk);
      return true;
    }
  }
  char nlabel[11];
  std::snprintf(nlabel, sizeof(nlabel), "##%d-input", n);
  InputText(nlabel, newItem);
  SameLine();
  if (Button("+##list-edit-add")) {
    values.push_back(newItem);
    newItem = "";
    return true;
  }
  return false;
}

bool Combo(const char *label, int *currIndex,
           std::vector<std::string> &values) {
  if (values.empty()) {
    return false;
  }
  return Combo(label, currIndex, vector_getter,
               reinterpret_cast<void *>(&values), values.size());
}

bool ListBox(const char *label, int *currIndex,
             std::vector<std::string> &values) {
  if (values.empty()) {
    return false;
  }
  return ListBox(label, currIndex, vector_getter,
                 reinterpret_cast<void *>(&values), values.size());
}

bool InputText(const char *label, std::string &str, size_t maxInputSize,
               ImGuiInputTextFlags flags, ImGuiInputTextCallback callback,
               void *user_data) {
  if (str.size() > maxInputSize) { // too large for editing
    ImGui::Text(str.c_str());
    return false;
  }

  std::string buffer(str);
  buffer.resize(maxInputSize);
  bool changed = ImGui::InputText(label, &buffer[0], maxInputSize, flags,
                                  callback, user_data);
  // using string as char array
  if (changed) {
    auto i = buffer.find_first_of('\0');
    str = buffer.substr(0u, i);
  }
  return changed;
}

} // end of namespace ImGui
