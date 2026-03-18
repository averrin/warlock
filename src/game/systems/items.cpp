#include <algorithm>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/components/resource_patch.hpp>
#include <game/state.hpp>
#include <game/systems/items.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <utils/entt.hpp>

namespace {
std::vector<std::pair<int, int>> getFrameOccupiedCells(const wl::transform& t, FrameSize size) {
  const int CELL = 75;
  int gridX = static_cast<int>(t.position.x) / CELL;
  int gridY = static_cast<int>(t.position.y) / CELL;
  int cells = 1;
  switch (size) {
    case FrameSize::S: cells = 1; break;
    case FrameSize::M: cells = 2; break;
    case FrameSize::L: cells = 3; break;
    case FrameSize::G: cells = 4; break;
  }
  
  std::vector<std::pair<int, int>> result;
  for (int dy = 0; dy < cells; ++dy) {
    for (int dx = 0; dx < cells; ++dx) {
      result.emplace_back(gridX + dx, gridY + dy);
    }
  }
  return result;
}

bool cellsOverlap(const std::vector<std::pair<int, int>>& a, 
                  const std::vector<std::pair<int, int>>& b) {
  for (const auto& ca : a) {
    for (const auto& cb : b) {
      if (ca.first == cb.first && ca.second == cb.second) return true;
    }
  }
  return false;
}
} // namespace

void ItemsSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();

  for (auto &f : current_state.registry.view<Frame>()) {
    auto &frame = current_state.registry.get<Frame>(f);
    std::vector<std::shared_ptr<Component>> storages = {};
    for (auto &c : frame.components) {
      if (c->data.get_or<int>("slots", 0) > 0) {
        if (c->storage == nullptr) {
          c->storage = std::make_shared<ItemStorage>(c->data.get<int>("slots"));
        }
        storages.push_back(c);
        break;
      }
    }
    for (auto &c : frame.components) {
      // Miner-patch overlap detection
      if (c->data.get<std::string>("type") == "Miner" && c->state == ComponentState::ACTIVE) {
        if (current_state.registry.valid(f) && current_state.registry.all_of<wl::transform>(f)) {
          auto& frame_transform = current_state.registry.get<wl::transform>(f);
          auto frame_cells = getFrameOccupiedCells(frame_transform, frame.size);
          
          for (auto patch_e : current_state.registry.view<ResourcePatch>()) {
            auto& patch = current_state.registry.get<ResourcePatch>(patch_e);
            if (cellsOverlap(frame_cells, patch.cells)) {
              // Miner overlaps this patch - can extract patch.item_name
              break;
            }
          }
        }
      }

      if (c->data.get_or<std::string>("recipe", "") != "" &&
          c->state == ComponentState::ACTIVE) {
        auto recipe_name = c->data.get_or<std::string>("recipe", "");
        if (recipe_name == "") {
          continue;
        }
        auto recipe = std::find_if(loader->get_recipes().begin(),
                                   loader->get_recipes().end(),
                                   [recipe_name](const RecipeDefinition &r) {
                                     return r.name == recipe_name;
                                   });

        if (recipe != loader->get_recipes().end()) {
          if (c->data.attributes.find("consumption") !=
              c->data.attributes.end()) {
            auto consumption = c->data.attributes["consumption"];
            if (modifiers.find(recipe_name) == modifiers.end() &&
                recipe->powerCost != 0.0f) {
              auto modifier = std::make_shared<Modifier>(
                  recipe_name, [=](const Attribute &attr) {
                    return std::get<float>(attr.GetBaseValue()) +
                           recipe->powerCost;
                  });
              modifiers[recipe_name] = modifier;
            }

            if (lastRecipe.find(c->data.id) == lastRecipe.end() ||
                recipe_name != lastRecipe[c->data.id]) {
              if (modifiers.find(lastRecipe[c->data.id]) != modifiers.end()) {
                // fmt::print("Removing modifier for {}\n",
                //            lastRecipe[c->data.id]);
                consumption->RemoveModifier(modifiers[lastRecipe[c->data.id]]);
              }
              if (modifiers.find(recipe_name) != modifiers.end()) {
                // fmt::print("Setting modifier for {}\n", recipe_name);
                consumption->AddModifier(modifiers[recipe_name]);
              }
            }
          }
          lastRecipe[c->data.id] = recipe_name;

          if (storages.size() == 0) {
            // fmt::print("No storage found for item producer {}\n",
            // c->data.id);
            continue;
          }

          auto entity = c->data.id;
          double timecost = recipe->timeCost * 1000;
          lastExecutionTime[entity] += targetInterval;

          if (lastExecutionTime[entity] >= timecost) {

            auto satisfied = false;
            auto inputs_count = recipe->inputs.size();
            if (recipe->inputs.size() == 0) {
              satisfied = true;
            }
            for (auto input : recipe->inputs) {
              auto stack = ItemStack(input.item, input.amount);
              for (auto storage : storages) {
                if (storage->storage->canRemove(stack)) {
                  inputs_count--;
                  break;
                }
              }
            }

            if (inputs_count == 0) {
              satisfied = true;
            }
            if (!satisfied) {
              auto prev = c->state;
              c->state = ComponentState::COMP_ERROR;
              c->error = "Not enough input components";
              auto &emitter = entt::locator<event_emitter>::value();
              emitter.publish(component_state_changed{
                c->frame_id, c->data.id, c->data.name,
                static_cast<int>(prev), static_cast<int>(c->state), c->error
              });
              lastExecutionTime[entity] = 0.0;
              continue;
            }

            for (auto input : recipe->inputs) {
              auto stack = ItemStack(input.item, input.amount);
              for (auto storage : storages) {
                if (storage->storage->remove(stack)) {
                  break;
                }
              }
            }

            satisfied = false;
            auto outputs_count = recipe->outputs.size();
            if (recipe->outputs.size() == 0) {
              satisfied = true;
            }
            for (auto output : recipe->outputs) {
              auto stack = ItemStack(output.item, output.amount);
              for (auto storage : storages) {
                if (storage->storage->canAdd(stack)) {
                  outputs_count--;
                  break;
                }
              }
            }

            if (outputs_count == 0) {
              satisfied = true;
            }
            if (!satisfied) {
              auto prev = c->state;
              c->state = ComponentState::COMP_ERROR;
              c->error = "Not enough space for output components";
              auto &emitter = entt::locator<event_emitter>::value();
              emitter.publish(component_state_changed{
                c->frame_id, c->data.id, c->data.name,
                static_cast<int>(prev), static_cast<int>(c->state), c->error
              });
              lastExecutionTime[entity] = 0.0;
              continue;
            }

            for (auto output : recipe->outputs) {
              auto stack = ItemStack(output.item, output.amount);
              for (auto storage : storages) {
                if (storage->storage->add(stack)) {
                  break;
                }
              }
            }

            // Reset the timer
            lastExecutionTime[entity] = 0.0;
          }
        } else {

          if (lastRecipe.find(c->data.id) == lastRecipe.end() ||
              recipe_name != lastRecipe[c->data.id]) {
            if (modifiers.find(lastRecipe[c->data.id]) != modifiers.end()) {
              auto consumption = c->data.attributes["consumption"];
              consumption->RemoveModifier(modifiers[lastRecipe[c->data.id]]);
            }
            lastRecipe[c->data.id] = recipe_name;
          }
        }
      }
    }
  }
}
