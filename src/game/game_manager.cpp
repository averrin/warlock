#include <chrono>
#include <filesystem>
#include <game/game_manager.hpp>
#include <game/oracle.hpp>
#include <magic_enum.hpp>
#include <mutex>
#include <thread>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
namespace fs = std::filesystem;
using namespace std::this_thread;     // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <game/meta_data.hpp>
#include <game/patch_loader.hpp>
#include <game/state.hpp>
#include <utils/assets_loader.hpp>
#include <utils/data/loader.hpp>

#include <algorithm> // For std::ranges::transform
#include <fmt/ranges.h>
#include <sstream>
#include <iomanip>
#include <game/components/frame.hpp>
#include <game/systems/environment.hpp>
#include <game/systems/input.hpp>
#include <game/systems/items.hpp>
#include <game/systems/power.hpp>
#include <game/systems/presentation.hpp>
#include <game/systems/thermal.hpp>
#include <game/systems/tweening.hpp>
#include <rpc/dto.hpp>
#include <rpc/server.hpp>
#include <ranges> // For ranges
#include <utils/entt_tools.hpp>

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

void Metadata::reconcileGlobalIdCounter(entt::registry &registry) {
  int max_id = entt::monostate<"id"_hs>{};
  auto bump = [&](int id) {
    if (id > max_id)
      max_id = id;
  };
  for (auto e : registry.view<Frame>()) {
    auto &f = registry.get<Frame>(e);
    bump(f.data.id);
    for (const auto &c : f.components) {
      if (!c)
        continue;
      bump(c->data.id);
      if (c->storage) {
        for (const auto &slot : c->storage->slots) {
          bump(slot.id);
          if (slot.stack)
            bump(slot.stack->id);
        }
      }
    }
  }
  for (auto e : registry.view<Connection>()) {
    bump(registry.get<Connection>(e).data.id);
  }
  entt::monostate<"id"_hs>{} = max_id;
}

namespace {

nlohmann::json serializeEnvironmentWithHistory(const Environment& env) {
  auto result = rpc::serializeEnvironment(env);
  auto toJsonArray = [](const std::deque<float>& values) {
    nlohmann::json out = nlohmann::json::array();
    for (float value : values) {
      out.push_back(value);
    }
    return out;
  };
  const auto findHistory = [&env](const std::string& key) -> const std::deque<float>* {
    auto it = env.named_history.find(key);
    if (it == env.named_history.end()) {
      return nullptr;
    }
    return &it->second;
  };

  nlohmann::json history = nlohmann::json::object();
  if (const auto* values = findHistory("temperature")) {
    history["temperature"] = toJsonArray(*values);
  } else {
    history["temperature"] = nlohmann::json::array();
  }
  if (const auto* values = findHistory("air_flow")) {
    history["air_flow"] = toJsonArray(*values);
  } else if (const auto* values = findHistory("airFlow")) {
    history["air_flow"] = toJsonArray(*values);
  } else {
    history["air_flow"] = nlohmann::json::array();
  }
  if (const auto* values = findHistory("sun")) {
    history["sun"] = toJsonArray(*values);
  } else {
    history["sun"] = nlohmann::json::array();
  }
  result["history"] = history;
  return result;
}

nlohmann::json serializePowerNetworks() {
  nlohmann::json networks = nlohmann::json::array();
  if (!entt::locator<PowerInfo>::has_value()) {
    return networks;
  }
  auto& info = entt::locator<PowerInfo>::value();
  for (auto& net : info.networks) {
    nlohmann::json frames_arr = nlohmann::json::array();
    for (auto frameId : net.frames) {
      frames_arr.push_back(frameId);
    }
    nlohmann::json history_obj = nlohmann::json::object();
    for (auto& [k, dq] : net.history) {
      nlohmann::json h = nlohmann::json::array();
      for (auto v : dq) {
        h.push_back(v);
      }
      history_obj[k] = h;
    }
    networks.push_back({
      {"name", net.data.name},
      {"frames", frames_arr},
      {"production", net.production},
      {"consumption", net.consumption},
      {"accumulated", net.accumulated},
      {"accumulated_available", net.accumulated_available},
      {"battery_count", net.battery_count},
      {"history", history_obj}
    });
  }
  return networks;
}

} // namespace

GameManager::GameManager() {
  log.is_debug = entt::monostate<"debug"_hs>{};
  startJob = std::make_shared<Job>("GameManager start",
                                   std::bind(&GameManager::start, this));
  entt::locator<std::mutex *>::emplace();
  entt::locator<Oracle>::emplace();
}

GameManager::~GameManager() {}

void GameManager::loadData() {
  auto p = log.parent;
  log.setParent(nullptr);
  log.setAsync(true);

  // Hold the update mutex to prevent systems from ticking with stale entities
  std::lock_guard<std::recursive_mutex> lock(updateMutex);

  log.start("Loading MetaData");
  started = false;

  auto &lua = entt::locator<sol::state>::value();
  auto loader = entt::locator<Loader>::value();

  auto &metaData = entt::locator<MetaData>::emplace();
  auto files =
      lua["settings"]["meta_data_files"].get<std::vector<std::string>>();
  loader.load<MetaData>(metaData, files);
  log.var("Probabilities", metaData.probability.size());
  log.var("MapFeatures", metaData.mapFeatures.size());
  log.stop("Loading MetaData");
  startJob->progress += 5;

  log.start("Loading Prototypes");
  auto &prototypes = entt::locator<Prototypes>::emplace();
  auto proto_files =
      lua["settings"]["proto_files"].get<std::vector<std::string>>();
  loader.load<Prototypes>(prototypes, proto_files);
  log.var("Prototypes", prototypes.registry.storage<hf::meta>().size());
  log.stop("Loading Prototypes");
  startJob->progress += 5;

  log.start("Loading State");
  auto &current_state = entt::locator<State>::emplace();

  fs::path PATH = entt::monostate<"path"_hs>{};
  auto state_path =
      PATH / fs::path(lua["settings"]["current_state"].get<std::string>());
  if (fs::exists(state_path) == false) {
    log.info("Creating new current state");
    auto init_state = std::make_shared<State>();
    auto init_state_files =
        lua["settings"]["init_states"].get<std::vector<std::string>>();
    loader.load<State>(*init_state, init_state_files);
    log.var("Init State", init_state->registry.storage<hf::meta>().size());
    auto store = current_state.create("current", state_path);
    store->initEmpty();
    EnttTools::copyRegistry(init_state->registry, store->registry);
    current_state.add(store);
    log.var("State Path", current_state.stores.front()->path.string());
  } else {
    log.info("Loading current state");
    loader.load<State>(current_state, {state_path.string()});
    log.var("Current State", current_state.registry.storage<hf::meta>().size());
  }
  log.stop("Loading State");
  startJob->progress += 5;

  log.start("Loading Patch Types");
  auto& patch_loader = entt::locator<PatchLoader>::emplace();
  fs::path patches_path = PATH / "scripts" / "patches";
  patch_loader.load_patches(patches_path.string(), lua);
  log.var("Patch Types", patch_loader.get_patch_types().size());
  log.stop("Loading Patch Types");

  // Repopulate WellKnownEntities from the newly-loaded registry
  WellKnownEntities wk;
  for (auto e : current_state.registry.view<Environment>()) {
    wk.environment = e;
    break;
  }
  for (auto e : current_state.registry.view<hf::meta>()) {
    auto &meta = current_state.registry.get<hf::meta>(e);
    if (meta.name == "Frames") wk.frames_folder = e;
    if (meta.name == "Connections") wk.connections_folder = e;
  }
  entt::locator<WellKnownEntities>::emplace(wk);

  Metadata::reconcileGlobalIdCounter(current_state.registry);

  started = true;

  if (exec) {
    refresh_nexus_component_apis(*exec);
  }

  log.setAsync(false);
  log.setParent(p);
}

void GameManager::saveData() {
  auto p = log.parent;
  log.setParent(nullptr);
  log.setAsync(true);

  auto label = "Saving data";
  log.start(label);
  auto loader = entt::locator<Loader>::value();
  auto &metaData = entt::locator<MetaData>::value();
  loader.save(metaData);
  auto &prototypes = entt::locator<Prototypes>::value();
  loader.save(prototypes);

  auto &state = entt::locator<State>::value();
  if (!state.stores.empty()) {
    // Use saveStateToFile to write the live registry (not stale store data)
    loader.saveStateToFile(state, state.stores.front()->path.string());
  }
  log.stop(label);

  {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
    last_saved_at_ = oss.str();
    last_save_ = hr_clock::now();
  }

  log.setAsync(false);
  log.setParent(p);
}

void GameManager::init(LibLog::Logger &parentLog) {
  log.setParent(&parentLog);
  auto label = "Initializing GameManager";
  log.start(label);
  auto &emitter = entt::locator<event_emitter>::value();
  auto &lua = entt::locator<sol::state>::value();

  auto &registry = entt::locator<entt::registry>::emplace();
  registry.on_construct<hf::script>().connect<&GameManager::initScript>(this);
  registry.on_destroy<hf::script>().connect<&GameManager::releaseScript>(this);

  lua.new_usertype<GameManager>(
      "GameManager", "new", sol::no_constructor, "addFrame",
      &GameManager::addFrame, "addConnection",
      sol::overload(
          sol::resolve<entt::entity(int, int, ConnectionType)>(&GameManager::addConnection),
          sol::resolve<entt::entity(int, int, ConnectionType, ConnectionMedium)>(
              &GameManager::addConnection)),
      "addFrameFromBlueprint", &GameManager::addFrameFromBlueprint);

  lua.set("gm", this);

  lastUpdate = hr_clock::now();
  emitter.publish(init_event{"game_manager"});
  log.stop(label);
}

entt::entity GameManager::addConnection(int source, int target, ConnectionType type) {
  return addConnection(source, target, type, ConnectionMedium::WIRE);
}

entt::entity GameManager::addConnection(int source, int target, ConnectionType type,
                                        ConnectionMedium medium) {
  std::lock_guard<std::recursive_mutex> lock(updateMutex);
  auto &current_state = entt::locator<State>::value();
  auto e = EnttTools::createEntityFromPrototype("CONNECTION",
                                                current_state.registry);
  auto &c = current_state.registry.get<Connection>(e);
  c.type = type;
  c.medium = medium;
  auto &meta = current_state.registry.get<hf::meta>(e);
  c.data.id = Metadata::newId();

  c.source = source;
  c.target = target;
  current_state.registry.emplace_or_replace<Connection>(e, c);
  auto name = fmt::format("[{}] {} -> {}", magic_enum::enum_name(c.type),
                          source, target);

  meta.name = name;
  meta.id = fmt::format("CONNECTION-{}", (int)e);
  current_state.registry.emplace_or_replace<hf::meta>(e, meta);

  auto &relation = current_state.registry.get_or_emplace<wl::relation>(e);
  auto &wk_conn = entt::locator<WellKnownEntities>::value();
  relation.parent = wk_conn.connections_folder;
  auto &p_relation =
      current_state.registry.get_or_emplace<wl::relation>(wk_conn.connections_folder);
  p_relation.children.push_back(e);
  return e;
}

int GameManager::addFrameFromBlueprint(std::string name) {
  std::lock_guard<std::recursive_mutex> lock(updateMutex);
  log.var("Adding frame from blueprint", name);
  auto &current_state = entt::locator<State>::value();
  std::string bp_source = exec->blueprints[name];
  if (bp_source.empty()) {
    throw std::runtime_error("Blueprint not found");
  }
  auto &lua = entt::locator<sol::state>::value();
  sol::table spec = lua.load(bp_source).call();
  auto e = addFrame(spec["name"].get_or<std::string>(""));
  auto &frame = current_state.registry.get<Frame>(e);
  frame.size = spec["size"].get_or(FrameSize::S);

  for (auto cn : spec["components"].get<std::vector<std::string>>()) {
    auto component = create_component_from_lua(exec->getState(frame.data.id),
                                               exec->getScript(cn));
    if (cn == "Core") {
      auto code = spec["code"].get_or<std::string>("");
      if (code != "") {
        component->data.set("code", code);
      }
    }
    frame.addComponent(component);
  }
  return frame.data.id;
}

entt::entity GameManager::addFrame(std::string name) {
  std::lock_guard<std::recursive_mutex> lock(updateMutex);
  auto &current_state = entt::locator<State>::value();
  auto e =
      EnttTools::createEntityFromPrototype("FRAME", current_state.registry);
  auto &frame = current_state.registry.get<Frame>(e);
  auto &meta = current_state.registry.get<hf::meta>(e);
  frame.data.id = Metadata::newId();
  frame.data.name = name;

  Attribute temp("Temperature", "Total frame temperature in Celsius",
                 AttributeType::FLOAT, -1000.0f);
  frame.data.attributes["temp"] = std::make_shared<Attribute>(temp);

  current_state.registry.emplace_or_replace<Frame>(e, frame);

  meta.name = fmt::format("{} [{}]", name, frame.data.id);
  meta.id = fmt::format("FRAME-{}", (int)e);
  current_state.registry.emplace_or_replace<hf::meta>(e, meta);

  auto &transform = current_state.registry.get_or_emplace<wl::transform>(e);
  auto snap = 128.0f;
  transform.position.x = Random::get(0, 800);
  transform.position.x = std::round(transform.position.x / snap) * snap;
  transform.position.y = Random::get(0, 800);
  transform.position.y = std::round(transform.position.y / snap) * snap;

  auto &relation = current_state.registry.get_or_emplace<wl::relation>(e);
  auto &wk_frames = entt::locator<WellKnownEntities>::value();
  relation.parent = wk_frames.frames_folder;
  auto &p_relation =
      current_state.registry.get_or_emplace<wl::relation>(wk_frames.frames_folder);
  p_relation.children.push_back(e);
  return e;
}

void GameManager::start() {
  auto p = log.parent;
  auto &emitter = entt::locator<event_emitter>::value();
  auto &lua = entt::locator<sol::state>::value();

  loadData();
  saveData();

  fs::path PATH = entt::monostate<"path"_hs>{};
  auto assetLoader = entt::locator<AssetLoader>::emplace((PATH / "assets").string());
  log.var("Assets", assetLoader.getTextures().size());

  log.setParent(nullptr);
  log.setAsync(true);
  auto label = "Location generation";
  log.start(label);
  auto &current_state = entt::locator<State>::value();

  emitter.publish(ready_event{"game_manager"});
  log.stop(label);

  systems.push_back(std::make_shared<TweeningSystem>());
  systems.push_back(std::make_shared<PowerSystem>());
  systems.push_back(std::make_shared<ThermalSystem>());
  systems.push_back(std::make_shared<EnvironmentSystem>());
  exec = std::make_shared<CodeExecutionSystem>();
  refresh_nexus_component_apis(*exec);
  systems.push_back(exec);
  items = std::make_shared<ItemsSystem>();
  systems.push_back(items);
  if (!headless) {
    systems.push_back(std::make_shared<PresentationSystem>());
    input = std::make_shared<InputSystem>();
    systems.push_back(input);
  }

  for (auto &c : exec->sources) {
    components.push_back(c.first);
  }

  emitter.connect<exec_lua_function>([=, this](const auto &e, const auto &em) {
    exec->executeCoreFunction(e.component, e.function_name);
  });

  emitter.connect<add_component>([=, this](const auto &e, const auto &em) {
    auto component = create_component_from_lua(
        exec->getState(e.frame.data.id), exec->getScript(e.component_name));
    e.frame.addComponent(component);
  });

  emitter.connect<input_selected_position>([&](auto &e, auto &em) {
    auto nf = addFrame("New Frame");
    auto &transform = current_state.registry.get<wl::transform>(nf);
    transform.position = e.position;
    current_state.registry.replace<wl::transform>(nf, transform);
  });

  if (current_state.registry.template storage<entt::entity>().size() > 0) {
    // Populate WellKnownEntities from existing registry
    WellKnownEntities wk;
    for (auto e : current_state.registry.view<Environment>()) {
      wk.environment = e;
      break;
    }
    for (auto e : current_state.registry.view<hf::meta>()) {
      auto &meta = current_state.registry.get<hf::meta>(e);
      if (meta.name == "Frames") wk.frames_folder = e;
      if (meta.name == "Connections") wk.connections_folder = e;
    }
    entt::locator<WellKnownEntities>::emplace(wk);
    log.setAsync(false);
    log.setParent(p);
    started = true;
    return;
  }

  WellKnownEntities wk;
  wk.environment = EnttTools::createEntityFromPrototype("ENV", current_state.registry);
  wk.frames_folder = EnttTools::createEntityFromPrototype("FOLDER", current_state.registry,
                                       "Frames");
  wk.connections_folder = EnttTools::createEntityFromPrototype("FOLDER", current_state.registry,
                                       "Connections");
  entt::locator<WellKnownEntities>::emplace(wk);
  lua.load_file((PATH / "scripts/game_init.lua").string()).call();

  log.setAsync(false);
  log.setParent(p);
  started = true;
}

void GameManager::enqueueCommand(std::function<void()> cmd) {
  std::lock_guard<std::mutex> lock(command_mutex_);
  pending_commands_.push(std::move(cmd));
}

void GameManager::serve() {
  if (!started)
    return;

  {
    auto &st = entt::locator<State>::value();
    if (st.registry.template storage<entt::entity>().size() == 0) {
      return;
    }
  }

  {
    std::lock_guard<std::mutex> lock(command_mutex_);
    while (!pending_commands_.empty()) {
      pending_commands_.front()();
      pending_commands_.pop();
    }
  }

  // Re-check after commands (loadData may have replaced State)
  if (!started) return;
  auto &current_state = entt::locator<State>::value();
  if (current_state.registry.template storage<entt::entity>().size() == 0) {
    return;
  }

  tick_count_++;
  if (!paused_) {
    updateMutex.lock();

    // Enforce simple component requirements defined in Lua specs.
    {
      auto &state = entt::locator<State>::value();
      auto &registry = state.registry;
      auto &emitter = entt::locator<event_emitter>::value();

      auto view = registry.view<Frame>();
      for (auto entity : view) {
        auto &frame = view.get<Frame>(entity);
        for (auto &comp : frame.components) {
          if (!comp) continue;
          if (comp->require.empty()) continue;
          // Only enforce for active / activating components.
          if (comp->state != ComponentState::ACTIVE &&
              comp->state != ComponentState::ACTIVATING) {
            continue;
          }

          std::vector<std::string> missing;
          for (const auto &needed : comp->require) {
            bool found = false;
            for (const auto &other : frame.components) {
              if (!other) continue;
              if (other.get() == comp.get()) continue;
              if (other->data.get<std::string>("type") == needed) {
                found = true;
                break;
              }
            }
            if (!found) {
              missing.push_back(needed);
            }
          }

          if (!missing.empty()) {
            auto message =
                fmt::format("Missing required components: {}", fmt::join(missing, ", "));
            if (comp->state != ComponentState::COMP_ERROR ||
                comp->error != message) {
              auto prev = comp->state;
              comp->state = ComponentState::COMP_ERROR;
              comp->error = message;
              emitter.publish(component_state_changed{
                  comp->frame_id,
                  comp->data.id,
                  comp->data.name,
                  static_cast<int>(prev),
                  static_cast<int>(comp->state),
                  comp->error});
            }
          }
        }
      }
    }

    for (size_t i = 0; i < systems.size(); ++i) {
      const std::chrono::duration<double, std::milli> delta =
          hr_clock::now() - lastUpdate;
      const std::chrono::duration<double, std::milli> scaled =
          delta * static_cast<double>(speed_multiplier_);
      if (systems[i]->enabled) {
        systems[i]->update(scaled);
      }
    }
    lastUpdate = hr_clock::now();
    updateMutex.unlock();
  }

  // Broadcast state (including storage) to all connected clients
  if (entt::locator<rpc::Server>::has_value()) {
    auto& rpcServer = entt::locator<rpc::Server>::value();
    if (rpcServer.clientCount() > 0) {
      auto now = hr_clock::now();
      if (last_state_push_.time_since_epoch().count() == 0 ||
          now - last_state_push_ >= std::chrono::milliseconds(200)) {
        std::lock_guard<std::recursive_mutex> lock(updateMutex);
        auto& registry = current_state.registry;
        nlohmann::json frames = nlohmann::json::array();
        auto frameView = registry.view<Frame>();
        for (auto entity : frameView) {
          auto& frame = frameView.get<Frame>(entity);
          frames.push_back(rpc::serializeFrame(registry, entity, frame));
        }

        nlohmann::json connections = nlohmann::json::array();
        auto connView = registry.view<Connection>();
        for (auto entity : connView) {
          auto& conn = connView.get<Connection>(entity);
          connections.push_back(rpc::serializeConnection(entity, conn, items.get()));
        }

        rpcServer.broadcast("event.state_update", {
          {"tick", tick_count_},
          {"frames", frames},
          {"connections", connections}
        });
        last_state_push_ = now;
      }

      if (last_env_push_.time_since_epoch().count() == 0 ||
          now - last_env_push_ >= std::chrono::milliseconds(500)) {
        std::lock_guard<std::recursive_mutex> lock(updateMutex);
        auto& registry = current_state.registry;
        auto& wk = entt::locator<WellKnownEntities>::value();
        if (wk.environment != entt::null && registry.valid(wk.environment) && registry.all_of<Environment>(wk.environment)) {
          rpcServer.broadcast("event.env_update", {
            {"env", serializeEnvironmentWithHistory(registry.get<Environment>(wk.environment))}
          });
        }
        last_env_push_ = now;
      }

      if (last_power_push_.time_since_epoch().count() == 0 ||
          now - last_power_push_ >= std::chrono::milliseconds(500)) {
        std::lock_guard<std::recursive_mutex> lock(updateMutex);
        rpcServer.broadcast("event.power_update", {
          {"networks", serializePowerNetworks()}
        });
        last_power_push_ = now;
      }
    }
  }

  // Autosave
  if (autosave_enabled_ && started) {
    auto now = hr_clock::now();
    if (last_save_.time_since_epoch().count() == 0 ||
        now - last_save_ >= autosave_interval_) {
      saveData();
      if (entt::locator<rpc::Server>::has_value()) {
        auto& rpcServer = entt::locator<rpc::Server>::value();
        rpcServer.broadcast("notify.state.saved", {
          {"saved_at", last_saved_at_},
          {"auto", true}
        });
      }
    }
  }
}

void GameManager::initScript(entt::registry &registry, entt::entity entity) {
  fs::path PATH = entt::monostate<"path"_hs>{};
  registry.patch<hf::script>(entity, [&](auto &script) {
    auto &lua = entt::locator<sol::state>::value();
    log.var("Script", script.path);
    script.self = lua.load_file((PATH / script.path).string()).call();
    script.self["id"] = entity;
    script.handlers.init = script.self["init"];
    script.handlers.create = script.self["create"];
    script.handlers.destroy = script.self["destroy"];
    script.handlers.update = script.self["update"];
  });
  auto &script = registry.get<hf::script>(entity);
  if (!script.enabled)
    return;
  script.handlers.init(script.self);
}

void GameManager::releaseScript(entt::registry &registry, entt::entity entity) {
  auto script = registry.get<hf::script>(entity);
  if (!script.enabled)
    return;
  script.handlers.destroy(script.self);
}

void GameManager::startFramePlacement() {
  input->startPositionSelection("FRAME_GHOST_M", 128);
}
