#include <editors/state_editor.hpp>
#include <utils/data/loader.hpp>

StateEditor::StateEditor(std::string name) {
  et_editor = std::make_shared<EntityTreeEditor>("Prototype Editor");
  startJob = std::make_shared<Job>("Opening states for edition",
                                   std::bind(&StateEditor::start, this));
  log.setAsync(true);
  log.setOffset(1);
  log.info("StateEditor created");
}

void StateEditor::start() {
  started = false;
  // auto label = "Open States";
  // log.start(label);

  auto &lua = entt::locator<sol::state>::value();
  auto pathes = lua["editor"]["loaded_states"].get<std::vector<std::string>>();
  auto &loader = entt::locator<Loader>::value();
  fs::path PATH = entt::monostate<"path"_hs>{};
  for (auto &file : pathes) {
    auto state = std::make_shared<State>();
    auto success = loader.load<State>(*state, {(PATH / file).string()});
    if (!success) {
      log.error("Failed to load state: {}", file);
      continue;
    }
    states.push_back(state);
  }

  // log.stop(label);
  started = true;
}

void StateEditor::render() {
  auto &gm = entt::locator<GameManager>::value();
  if (!started || !gm.started) {
    return;
  }
  ImGui::Begin("State Editor");

  ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
  if (ImGui::BeginTabBar("StoresTabs", tab_bar_flags)) {

    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0.65f, 0.15f, 0.15f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_TabActive, ImVec4(1.f, 0.2f, 0.2f, 1.f));
    if (ImGui::BeginTabItem("Active")) {
      ImGui::PopStyleColor(3);
      auto &active_state = entt::locator<State>::value();
      if (ImGui::Button("Save")) {
        auto &loader = entt::locator<Loader>::value();
        loader.save(active_state);
      }
      ImGui::Separator();
      et_editor->renderInline<State, hf::meta>(active_state);
      ImGui::EndTabItem();
    } else {
      ImGui::PopStyleColor(3);
    }
    for (auto state : states) {
      if (ImGui::BeginTabItem(state->stores.front()->name.c_str())) {

        if (ImGui::Button("Activate")) {
          entt::locator<State>::emplace(*state);
        }
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
          auto &loader = entt::locator<Loader>::value();
          loader.save(*state);
        }
        ImGui::Separator();

        et_editor->renderInline<State, hf::meta>(*state);
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }

  ImGui::End();
}
