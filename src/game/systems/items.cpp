#include <algorithm>
#include <filesystem>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <game/frame_deposit_query.hpp>
#include <game/components/resource_patch.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/systems/items.hpp>
#include <liblog/liblog.hpp>
#include <ranges> // For ranges
#include <unordered_map>
#include <utils/entt.hpp>

namespace {
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

bool frame_has_nexus_spendable_sink(Frame *f, int storage_component_id) {
  if (!f)
    return false;
  for (auto &c : f->components) {
    if (c->data.get_or<std::string>("type", "") != "Nexus")
      continue;
    int tid = c->data.get_or<int>("target", -1);
    if (tid < 0)
      tid = c->data.id;
    if (tid == storage_component_id)
      return true;
  }
  return false;
}

void drain_nexus_linked_spendable_storage(
    Frame &frame,
    const std::unordered_map<std::string, ItemDefinition> &item_map) {
  auto &gm = entt::locator<GameManager>::value();
  for (auto &nexus : frame.components) {
    if (nexus->data.get_or<std::string>("type", "") != "Nexus")
      continue;
    int tid = nexus->data.get_or<int>("target", -1);
    if (tid < 0)
      tid = nexus->data.id;
    std::shared_ptr<Component> storage_comp = nullptr;
    for (auto &c : frame.components) {
      if (c->data.id == tid && c->storage) {
        storage_comp = c;
        break;
      }
    }
    if (!storage_comp)
      continue;
    for (auto &slot : storage_comp->storage->slots) {
      if (!slot.stack || slot.stack->amount <= 0)
        continue;
      auto def_it = item_map.find(slot.stack->item.name);
      if (def_it == item_map.end() || !def_it->second.spendable)
        continue;
      const std::string name = slot.stack->item.name;
      const int amt = slot.stack->amount;
      ItemStack rm(slot.stack->item, amt);
      if (storage_comp->storage->remove(rm))
        gm.addSpendable(name, amt);
    }
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

std::shared_ptr<Modifier> ItemsSystem::sharedRecipeModifier(const std::string &recipeName) {
  if (!loader) {
    return nullptr;
  }
  const auto &recipes = loader->get_recipes();
  auto recipe =
      std::find_if(recipes.begin(), recipes.end(),
                   [&](const RecipeDefinition &r) { return r.name == recipeName; });
  if (recipe == recipes.end() || recipe->powerCost == 0.0f) {
    return nullptr;
  }
  auto it = modifiers.find(recipeName);
  if (it != modifiers.end()) {
    return it->second;
  }
  const float pc = recipe->powerCost;
  auto mod = std::make_shared<Modifier>(recipeName, [=](const Attribute &attr) {
    return std::get<float>(attr.GetBaseValue()) + pc;
  });
  modifiers[recipeName] = mod;
  return mod;
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
        const auto &item_map = loader->get_items();
        auto def_it = item_map.find(itemToMove->item.name);
        const bool is_spendable =
            def_it != item_map.end() && def_it->second.spendable;
        const bool nexus_sink = is_spendable && frame_has_nexus_spendable_sink(
                                                     rf, receiverStorage->data.id);

        ItemStack singleItem(itemToMove->item, 1);
        if (nexus_sink) {
          if (senderStorage->storage->canRemove(singleItem)) {
            senderStorage->storage->remove(singleItem);
            auto &gm = entt::locator<GameManager>::value();
            gm.addSpendable(itemToMove->item.name, 1);
            conveyorExecutionTime[entity] -= timecost;
            applyEdgeProgress();
          } else {
            conveyorExecutionTime[entity] = timecost;
            applyEdgeProgress();
          }
        } else if (receiverStorage->storage->canAdd(singleItem)) {
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
    drain_nexus_linked_spendable_storage(frame, loader->get_items());
    for (auto &c : frame.components) {
      if (c->data.get_or<std::string>("recipe", "") != "" &&
          c->state == ComponentState::ACTIVE) {
        auto recipe_name = c->data.get_or<std::string>("recipe", "");
        if (recipe_name == "") {
          continue;
        }
        auto recipe_end = loader->get_recipes().end();
        auto recipe = std::find_if(loader->get_recipes().begin(), recipe_end,
                                   [recipe_name](const RecipeDefinition &r) {
                                     return r.name == recipe_name;
                                   });

        bool recipe_valid = (recipe != recipe_end);
        if (recipe_valid && c->data.get<std::string>("type") == "Miner") {
          auto deposits =
              depositItemsUnderFrame(current_state.registry, f, frame);
          bool produces_deposit = false;
          for (const auto &out : recipe->outputs) {
            if (deposits.count(out.item.name)) {
              produces_deposit = true;
              break;
            }
          }
          if (!produces_deposit) {
            recipe_valid = false;
          }
        }

        if (recipe_valid) {
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
                consumption->RemoveModifier(modifiers[lastRecipe[c->data.id]]);
              }
              if (modifiers.find(recipe_name) != modifiers.end()) {
                consumption->AddModifier(modifiers[recipe_name]);
              }
            }
          }
          lastRecipe[c->data.id] = recipe_name;

          if (storages.size() == 0) {
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

            lastExecutionTime[entity] = 0.0;
          }
        } else {
          if (c->data.attributes.find("consumption") !=
              c->data.attributes.end()) {
            auto consumption = c->data.attributes["consumption"];
            if (lastRecipe.find(c->data.id) != lastRecipe.end()) {
              const std::string prev = lastRecipe[c->data.id];
              if (modifiers.find(prev) != modifiers.end()) {
                consumption->RemoveModifier(modifiers[prev]);
              }
            }
          }
          lastRecipe[c->data.id] = recipe_name;
        }
      }
    }
  }
}
