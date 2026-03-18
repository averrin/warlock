#include <rpc/handlers/storage_handler.hpp>
#include <rpc/handlers/handler_utils.hpp>
#include <game/game_manager.hpp>
#include <game/state.hpp>
#include <game/components/frame.hpp>
#include <game/item_loader.hpp>
#include <utils/entt.hpp>

namespace rpc {

static Component* findStorageComponent(Frame& frame, int component_id) {
  for (auto& c : frame.components) {
    if (c && c->data.id == component_id && c->storage) return c.get();
  }
  return nullptr;
}

static ItemSlot* findSlot(ItemStorage& storage, int slot_id) {
  for (auto& slot : storage.slots) {
    if (slot.id == slot_id) return &slot;
  }
  return nullptr;
}

void registerStorageHandlers(Server& server) {
  server.router().on("storage.add_slot", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    if (!params.contains("frame_id") || !params.contains("component_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing frame_id or component_id"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    for (auto e : registry.view<Frame>()) {
      if (registry.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
    }
    if (frame_entity == entt::null) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};

    auto& frame = registry.get<Frame>(frame_entity);
    auto* comp = findStorageComponent(frame, component_id);
    if (!comp) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found or has no storage"};

    ItemSlot slot;
    slot.id = Metadata::newId();
    comp->storage->slotsCount++;
    comp->storage->slots.push_back(slot);
    return {{"ok", true}, {"slot_id", slot.id}};
  });

  server.router().on("storage.set_slot", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    if (!params.contains("frame_id") || !params.contains("component_id") ||
        !params.contains("slot_id") || !params.contains("item_id") || !params.contains("amount")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required params"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    int slot_id = params["slot_id"].get<int>();
    std::string item_id = params["item_id"].get<std::string>();
    int amount = params["amount"].get<int>();

    if (amount <= 0) throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Amount must be positive"};

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    for (auto e : registry.view<Frame>()) {
      if (registry.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
    }
    if (frame_entity == entt::null) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};

    auto& frame = registry.get<Frame>(frame_entity);
    auto* comp = findStorageComponent(frame, component_id);
    if (!comp) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found or has no storage"};

    auto* slot = findSlot(*comp->storage, slot_id);
    if (!slot) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Slot not found"};
    if (slot->stack) throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Slot not empty"};

    ItemDefinition def;
    if (gm.items && gm.items->loader) {
      const auto& items = gm.items->loader->get_items();
      auto it = items.find(item_id);
      if (it == items.end()) throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Unknown item: " + item_id};
      def = it->second;
    } else {
      def.name = item_id;
      def.description = "";
      def.stack = 999;
    }

    int clamped = std::min(amount, def.stack);
    slot->stack = std::make_shared<ItemStack>(def, clamped);
    return {{"ok", true}};
  });

  server.router().on("storage.set_amount", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    if (!params.contains("frame_id") || !params.contains("component_id") ||
        !params.contains("slot_id") || !params.contains("amount")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required params"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    int slot_id = params["slot_id"].get<int>();
    int amount = params["amount"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    for (auto e : registry.view<Frame>()) {
      if (registry.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
    }
    if (frame_entity == entt::null) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};

    auto& frame = registry.get<Frame>(frame_entity);
    auto* comp = findStorageComponent(frame, component_id);
    if (!comp) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found or has no storage"};

    auto* slot = findSlot(*comp->storage, slot_id);
    if (!slot) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Slot not found"};
    if (!slot->stack) throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Slot is empty"};

    if (amount <= 0) {
      slot->stack = nullptr;
    } else {
      int max_val = slot->stack->item.stack;
      slot->stack->amount = std::min(amount, max_val);
    }
    return {{"ok", true}};
  });

  server.router().on("storage.clear_slot", [&server](const Context& ctx, const nlohmann::json& params) -> nlohmann::json {
    requireClaim(server, ctx);
    auto& gm = entt::locator<GameManager>::value();
    if (!gm.started) throw rpc::RpcError{rpc::error::INTERNAL_ERROR, "Game not started"};
    if (!params.contains("frame_id") || !params.contains("component_id") || !params.contains("slot_id")) {
      throw rpc::RpcError{rpc::error::INVALID_PARAMS, "Missing required params"};
    }
    int frame_data_id = params["frame_id"].get<int>();
    int component_id = params["component_id"].get<int>();
    int slot_id = params["slot_id"].get<int>();

    std::lock_guard<std::recursive_mutex> lock(gm.updateMutex);
    auto& state = entt::locator<State>::value();
    auto& registry = state.registry;

    entt::entity frame_entity = entt::null;
    for (auto e : registry.view<Frame>()) {
      if (registry.get<Frame>(e).data.id == frame_data_id) { frame_entity = e; break; }
    }
    if (frame_entity == entt::null) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Frame not found"};

    auto& frame = registry.get<Frame>(frame_entity);
    auto* comp = findStorageComponent(frame, component_id);
    if (!comp) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Component not found or has no storage"};

    auto* slot = findSlot(*comp->storage, slot_id);
    if (!slot) throw rpc::RpcError{rpc::error::ENTITY_NOT_FOUND, "Slot not found"};

    slot->stack = nullptr;
    return {{"ok", true}};
  });
}

} // namespace rpc
