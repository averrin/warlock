#include <algorithm>
#include <filesystem>
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
    case FrameSize::XS: cells = 1; break;
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

/** Neighbor→relay edges run the item timer; relay→neighbor edges are skipped in the loop.
 * Copy progress to the sibling connection so both legs animate on the canvas. */
void mirrorRelayOutgoingLeg(std::unordered_map<int, double> &progress,
                            const std::vector<Connection> &conns, const Connection &conn,
                            int relayFrameId, int neighborFrameId) {
  auto it = progress.find(conn.data.id);
  if (it == progress.end()) {
    return;
  }
  const double v = it->second;
  for (const auto &c2 : conns) {
    if (c2.data.id == conn.data.id) {
      continue;
    }
    if (c2.source != relayFrameId && c2.target != relayFrameId) {
      continue;
    }
    const int o = c2.source == relayFrameId ? c2.target : c2.source;
    if (o == neighborFrameId) {
      continue;
    }
    progress[c2.data.id] = v;
  }
}

void eraseRelayLegPair(std::unordered_map<int, double> &progress,
                       const std::vector<Connection> &conns, const Connection &conn,
                       int relayFrameId, int neighborFrameId) {
  progress.erase(conn.data.id);
  for (const auto &c2 : conns) {
    if (c2.data.id == conn.data.id) {
      continue;
    }
    if (c2.source != relayFrameId && c2.target != relayFrameId) {
      continue;
    }
    const int o = c2.source == relayFrameId ? c2.target : c2.source;
    if (o == neighborFrameId) {
      continue;
    }
    progress.erase(c2.data.id);
  }
}
} // namespace

ItemsSystem::ItemsSystem() : System(50, "Items") {
  loader = std::make_shared<ItemLoader>();
  namespace fs = std::filesystem;
  fs::path PATH = entt::monostate<"path"_hs>{};
  auto &lua = entt::locator<sol::state>::value();
  loader->load_items((PATH / "scripts" / "items").string(), lua);
  loader->load_recipes((PATH / "scripts" / "recipes").string(), lua);

  const auto &items = loader->get_items();
  const auto &recipes = loader->get_recipes();
  (void)items;
  (void)recipes;
}

void ItemsSystem::setConveyorProgressForConnection(int connDataId, int senderCompId,
                                                   double timecost) {
  conveyor_progress_by_connection_[connDataId] =
      std::clamp(conveyorExecutionTime[senderCompId] / timecost, 0.0, 1.0);
}

double ItemsSystem::conveyorTransferProgress(int connectionDataId) const {
  auto it = conveyor_progress_by_connection_.find(connectionDataId);
  if (it == conveyor_progress_by_connection_.end()) {
    return 0.0;
  }
  return it->second;
}

void ItemsSystem::fixedUpdate() {
  auto &current_state = entt::locator<State>::value();

  conveyor_progress_by_connection_.clear();

  auto frames_view = current_state.registry.view<Frame>();
  auto conns = std::vector<Connection>{};
  for (auto &c : current_state.registry.view<Connection>()) {
    auto conn = current_state.registry.get<Connection>(c);
    if (conn.type == ConnectionType::CONVEYOR) {
      conns.push_back(conn);
    }
  }

  for (auto &conn : conns) {
    Frame* sourceFrame = nullptr;
    Frame* targetFrame = nullptr;
    for (auto &f : frames_view) {
      auto &frame = current_state.registry.get<Frame>(f);
      if (frame.data.id == conn.source) sourceFrame = &frame;
      if (frame.data.id == conn.target) targetFrame = &frame;
    }
    if (!sourceFrame || !targetFrame) continue;

    std::shared_ptr<Component> sourceConnector = nullptr;
    std::shared_ptr<Component> targetConnector = nullptr;

    for (auto &c : sourceFrame->components) {
      if (c->data.get_or<std::string>("type", "") == "Conveyor Connector" &&
          c->state == ComponentState::ACTIVE) {
        sourceConnector = c;
        break;
      }
    }
    for (auto &c : targetFrame->components) {
      if (c->data.get_or<std::string>("type", "") == "Conveyor Connector" &&
          c->state == ComponentState::ACTIVE) {
        targetConnector = c;
        break;
      }
    }

    if (!sourceConnector || !targetConnector) continue;

    const bool srcRelay = sourceConnector->data.name == "Conveyor Relay";
    const bool tgtRelay = targetConnector->data.name == "Conveyor Relay";
    if (srcRelay && tgtRelay) continue;
    // relay -> neighbor: item flow is applied on the neighbor -> relay edge only
    if (srcRelay) continue;

    std::shared_ptr<Component> sender = nullptr;
    std::shared_ptr<Component> receiver = nullptr;

    if (!tgtRelay) {
      auto mode1 = sourceConnector->data.get_or<std::string>("mode", "SEND");
      auto mode2 = targetConnector->data.get_or<std::string>("mode", "SEND");
      if (mode1 == "SEND" && mode2 == "RECEIVE") {
        sender = sourceConnector;
        receiver = targetConnector;
      } else if (mode2 == "SEND" && mode1 == "RECEIVE") {
        sender = targetConnector;
        receiver = sourceConnector;
      } else {
        continue;
      }
    } else {
      if (sourceConnector->data.get_or<std::string>("mode", "SEND") != "SEND") continue;
      sender = sourceConnector;
      receiver = targetConnector;
    }

    if (!sender || !receiver) continue;

    float throughput = std::min(sender->data.get_or<float>("throughput", 1.0f),
                                receiver->data.get_or<float>("throughput", 1.0f));
    if (receiver->data.name == "Conveyor Relay") {
      float t = sender->data.get_or<float>("throughput", 1.0f);
      const int relayFid = receiver->frame_id;
      const int fromFid = sender->frame_id;
      for (const auto &c2 : conns) {
        if (c2.source != relayFid && c2.target != relayFid) continue;
        const int o = c2.source == relayFid ? c2.target : c2.source;
        if (o == fromFid) continue;
        Frame *otherF = nullptr;
        for (auto &f : frames_view) {
          auto &fr = current_state.registry.get<Frame>(f);
          if (fr.data.id == o) {
            otherF = &fr;
            break;
          }
        }
        if (!otherF) continue;
        for (auto &c : otherF->components) {
          if (c->data.get_or<std::string>("type", "") != "Conveyor Connector" ||
              c->state != ComponentState::ACTIVE)
            continue;
          t = std::min(t, c->data.get_or<float>("throughput", 1.0f));
          break;
        }
      }
      throughput = t;
    }

    if (throughput <= 0) continue;

    auto entity = sender->data.id;
    double timecost = (1.0 / throughput) * 1000.0;
    conveyorExecutionTime[entity] += targetInterval;
    const auto applyEdgeProgress = [&]() {
      setConveyorProgressForConnection(conn.data.id, entity, timecost);
      if (receiver->data.name == "Conveyor Relay") {
        mirrorRelayOutgoingLeg(conveyor_progress_by_connection_, conns, conn, receiver->frame_id,
                               sender->frame_id);
      }
    };
    applyEdgeProgress();

    if (conveyorExecutionTime[entity] >= timecost) {
      auto senderTargetId = sender->data.get_or<int>("target", -1);
      auto receiverTargetId = receiver->data.get_or<int>("target", -1);

      std::shared_ptr<Component> senderStorage = nullptr;
      std::shared_ptr<Component> receiverStorage = nullptr;

      Frame* sf = (sender == sourceConnector) ? sourceFrame : targetFrame;
      Frame* rf = (receiver == targetConnector) ? targetFrame : sourceFrame;

      for (auto &c : sf->components) {
        if (c->data.id == senderTargetId && c->storage) {
          senderStorage = c;
          break;
        }
      }
      for (auto &c : rf->components) {
        if (c->data.id == receiverTargetId && c->storage) {
          receiverStorage = c;
          break;
        }
      }

      // Conveyor relay: no storage — deliver to other neighbor's RECEIVE target storage.
      if (!receiverStorage && receiver->data.name == "Conveyor Relay") {
        const int relayFid = receiver->frame_id;
        const int fromFid = sender->frame_id;
        int otherNeighbor = -1;
        for (const auto &c2 : conns) {
          if (c2.source != relayFid && c2.target != relayFid)
            continue;
          const int o = c2.source == relayFid ? c2.target : c2.source;
          if (o != fromFid) {
            otherNeighbor = o;
            break;
          }
        }
        if (otherNeighbor >= 0) {
          Frame *bFrame = nullptr;
          for (auto &f : frames_view) {
            auto &fr = current_state.registry.get<Frame>(f);
            if (fr.data.id == otherNeighbor) {
              bFrame = &fr;
              break;
            }
          }
          if (bFrame) {
            std::shared_ptr<Component> bRecv = nullptr;
            for (auto &c : bFrame->components) {
              if (c->data.get_or<std::string>("type", "") != "Conveyor Connector" ||
                  c->state != ComponentState::ACTIVE)
                continue;
              if (c->data.get_or<std::string>("mode", "SEND") == "RECEIVE") {
                bRecv = c;
                break;
              }
            }
            if (bRecv) {
              const int bt = bRecv->data.get_or<int>("target", -1);
              for (auto &c : bFrame->components) {
                if (c->data.id == bt && c->storage) {
                  receiverStorage = c;
                  break;
                }
              }
            }
          }
        }
      }

      if (!senderStorage) {
        if (receiver->data.name == "Conveyor Relay") {
          eraseRelayLegPair(conveyor_progress_by_connection_, conns, conn, receiver->frame_id,
                            sender->frame_id);
        } else {
          conveyor_progress_by_connection_.erase(conn.data.id);
        }
        auto prev = sender->state;
        sender->state = ComponentState::COMP_ERROR;
        sender->error = "Sender target storage not found";
        auto &emitter = entt::locator<event_emitter>::value();
        emitter.publish(component_state_changed{
          sender->frame_id, sender->data.id, sender->data.name,
          static_cast<int>(prev), static_cast<int>(sender->state), sender->error
        });
        continue;
      }

      if (!receiverStorage) {
        if (receiver->data.name == "Conveyor Relay") {
          conveyorExecutionTime[entity] = timecost;
          applyEdgeProgress();
          if (receiver->state == ComponentState::COMP_ERROR) {
            auto prev = receiver->state;
            receiver->state = ComponentState::ACTIVE;
            receiver->error.clear();
            auto &emitter = entt::locator<event_emitter>::value();
            emitter.publish(component_state_changed{
              receiver->frame_id, receiver->data.id, receiver->data.name,
              static_cast<int>(prev), static_cast<int>(receiver->state), receiver->error
            });
          }
          continue;
        }
        conveyor_progress_by_connection_.erase(conn.data.id);
        auto prev = receiver->state;
        receiver->state = ComponentState::COMP_ERROR;
        receiver->error = "Receiver target storage not found";
        auto &emitter = entt::locator<event_emitter>::value();
        emitter.publish(component_state_changed{
          receiver->frame_id, receiver->data.id, receiver->data.name,
          static_cast<int>(prev), static_cast<int>(receiver->state), receiver->error
        });
        continue;
      }

      auto filter = sender->data.get_or<std::string>("filter", "");
      std::shared_ptr<ItemStack> itemToMove = nullptr;

      if (filter == "") {
        for (auto &slot : senderStorage->storage->slots) {
          if (slot.stack != nullptr && slot.stack->amount > 0) {
            itemToMove = slot.stack;
            break;
          }
        }
      } else {
        itemToMove = senderStorage->storage->getStackByItem(filter);
      }

      if (itemToMove && itemToMove->amount > 0) {
        ItemStack singleItem(itemToMove->item, 1);
        if (receiverStorage->storage->canAdd(singleItem)) {
          senderStorage->storage->remove(singleItem);
          // remove() mutates `singleItem.amount` to 0; add must use a fresh stack
          ItemStack toReceive(itemToMove->item, 1);
          receiverStorage->storage->add(toReceive);
          conveyorExecutionTime[entity] -= timecost;
          applyEdgeProgress();
        } else {
           // Output is full, reset or cap timer
           conveyorExecutionTime[entity] = timecost;
           applyEdgeProgress();
        }
      } else {
         // Input is empty, reset or cap timer
         conveyorExecutionTime[entity] = timecost;
         applyEdgeProgress();
      }
    }
  }

  for (auto &f : frames_view) {
    auto &frame = current_state.registry.get<Frame>(f);
    std::vector<std::shared_ptr<Component>> storages = {};
    for (auto &c : frame.components) {
      if (c->data.get_or<int>("slots", 0) > 0) {
        if (c->storage == nullptr) {
          c->storage = std::make_shared<ItemStorage>(c->data.get<int>("slots"));
        }
        storages.push_back(c);
      } else if (c->data.get_or<std::string>("type", "") == "Storage") {
        if (c->storage == nullptr) {
          c->storage = std::make_shared<ItemStorage>(4);
        }
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
