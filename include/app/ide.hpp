#ifndef __IDE_H_
#define __IDE_H_

#include <IconsFontAwesome6.h>
// #include <bits/fs_ops.h>
#include <TextEditor.h>
#include <fmt/format.h>
#include <fstream>
#include <imgui.h>
#include <iostream>
#include <sol/sol.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

class IDE {
  fs::path PATH;
  std::vector<fs::path> files;
  std::map<fs::path, std::string> content;
  std::string currentFile;
  std::string todoContent;

  // TextEditor::ErrorMarkers markers;
  std::shared_ptr<TextEditor> luaEditor;
  std::shared_ptr<TextEditor> todoEditor;

public:
  IDE() {
    luaEditor = std::make_shared<TextEditor>();
    todoEditor = std::make_shared<TextEditor>();
  };

  void addFolder(fs::path p) {
    for (const fs::path &file : fs::directory_iterator(p)) {
      // fmt::print("ide: {}\n", file.string());
      if (file.extension() == ".lua") {
        files.push_back(file);
      } else {
        addFolder(file);
      }
    }
  }

  void drawFolder(fs::path p, sol::state &lua, bool root = false) {
    if (root ||
        ImGui::TreeNode(
            fmt::format("##{}", p.string()).c_str(),
            fmt::format("{}  {}", ICON_FA_FOLDER, p.stem().string()).c_str())) {
      // if (fs::directory_iterator(p).size() == 0) {
      //   ImGui::Text("Empty folder");
      // }
      for (const fs::path &file : fs::directory_iterator(p)) {
        if (file.extension() == ".lua") {
          ImGui::Text(file.filename().c_str());
          ImGui::SameLine();
          if (ImGui::Button(
                  fmt::format("{}##{}", ICON_FA_FOLDER_OPEN, file.string())
                      .c_str())) {
            currentFile = file;
            luaEditor->SetText(content[currentFile]);
          }
          ImGui::SameLine();
          if (ImGui::Button(
                  fmt::format("{}##{}", ICON_FA_PLAY, file.string()).c_str())) {
            fmt::print("run {}", file.string());
            lua.script_file(file);
          }
        }
      }
      for (const fs::path &file : fs::directory_iterator(p)) {
        if (fs::is_directory(file)) {
          drawFolder(file, lua);
        }
      }
      if (!root) {
        ImGui::TreePop();
      }
    }
  }

  void init(fs::path p) {
    PATH = p;
    addFolder(PATH / "scripts");
    auto lang = TextEditor::LanguageDefinitionId::Lua;
    luaEditor->SetLanguageDefinition(lang);
    todoEditor->SetLanguageDefinition(lang);
    files.push_back(PATH / "todo.txt");

    for (auto fname : files) {
      std::ifstream ifs(fname);
      std::string _content((std::istreambuf_iterator<char>(ifs)),
                           (std::istreambuf_iterator<char>()));
      content[fname] = _content;
    }
    todoEditor->SetText(content[PATH / "todo.txt"]);
  }

  void renderTodo() {
    auto fname = PATH / "todo.txt";
    ImGui::Begin("TODO");

    if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
      std::ofstream out(fname);
      out << todoEditor->GetText();
      out.close();
    }

    todoEditor->Render("TODO Editor");
    content[fname] = todoEditor->GetText();
    ImGui::End();
  }
  void render() {
    auto &lua = entt::locator<sol::state>::value();
    ImGui::Begin("IDE tree");
    drawFolder(PATH / "scripts", lua, true);
    ImGui::End();

    ImGui::Begin("IDE");

    // ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
    // if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
    //   for (auto fname : files) {

    //     if (ImGui::BeginTabItem(fname.filename().c_str())) {
    // if (fname != currentFile) {
    //   luaEditor.SetText(content[fname]);
    //   currentFile = fname;
    // }

    if (ImGui::Button(ICON_FA_FLOPPY_DISK)) {
      std::ofstream out(currentFile);
      out << luaEditor->GetText();
      out.close();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLAY)) {
      // try {
      lua.script(luaEditor->GetText());
      // } catch (const std::exception &e) {
      // std::cout << e << std::endl;
      // }
    }

    luaEditor->ImGuiDebugPanel("Debug");
    luaEditor->Render("Lua Editor");
    content[currentFile] = luaEditor->GetText();
    // ImGui::EndTabItem();
    //     }
    //   }
    //   ImGui::EndTabBar();
    // }
    ImGui::End();
  }
};
#endif // __IDE_H_
