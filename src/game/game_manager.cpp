#include <chrono>
#include <filesystem>
#include <game/game_manager.hpp>
#include <game/frame_world.hpp>
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
#include <game/prototypes.hpp>
#include <game/state.hpp>
#include <utils/data/loader.hpp>

#include <algorithm> // For std::ranges::transform
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <sstream>
#include <iomanip>
#include <game/attributes.hpp>
#include <game/components/frame.hpp>
#include <game/spendable.hpp>
#include <game/systems/environment.hpp>
#include <game/systems/items.hpp>
#include <game/systems/power.hpp>
#include <game/systems/code_execution.hpp>
#include <game/data_link.hpp>
#include <game/systems/thermal.hpp>
#include <game/systems/tweening.hpp>
#include <game/systems/map_generator.hpp>
#include <game/mapgen_loader.hpp>
#include <game/components/resource_patch.hpp>
#include <game/systems/wireless_connection.hpp>
#include <rpc/dto.hpp>
#include <rpc/server.hpp>
#include <ranges> // For ranges
#include <utils/entt_tools.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <map>
#include <nlohmann/json.hpp>
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

static void stripInvalidFrameComponents(entt::registry &registry) {
  for (auto e : registry.view<Frame>()) {
    auto &frame = registry.get<Frame>(e);
    std::erase_if(frame.components, [](const std::shared_ptr<Component> &c) {
      return c == nullptr;
    });
    for (auto &c : frame.components) {
      std::erase_if(c->data.attributes,
                    [](const auto &p) { return p.second == nullptr; });
    }
  }
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

namespace {

fs::path legacy_spendables_json_path() {
  fs::path PATH = entt::monostate<"path"_hs>{};
  return PATH / "data" / "spendables.json";
}

void mergeLegacySpendablesJsonInto(std::map<std::string, int64_t> &pool) {
  auto path = legacy_spendables_json_path();
  if (!fs::exists(path)) {
    return;
  }
  std::ifstream ifs(path.string());
  if (!ifs) {
    return;
  }
  try {
    nlohmann::json j;
    ifs >> j;
    if (j.contains("amounts") && j["amounts"].is_object()) {
      for (auto it = j["amounts"].begin(); it != j["amounts"].end(); ++it) {
        pool[it.key()] = it.value().get<int64_t>();
      }
    }
  } catch (...) {
  }
}

void require_bootstrap_prototypes() {
  auto &prototypes = entt::locator<Prototypes>::value();
  for (auto e : prototypes.registry.view<hf::meta>()) {
    if (prototypes.registry.get<hf::meta>(e).id == "ENV")
      return;
  }
  throw std::runtime_error(
      "Frame prototypes failed to load (ENV missing). data/frame.proto is missing, empty, or "
      "unreadable from the working directory. Run `just init` so the build output has a valid "
      "junction to the repo `data/` folder, or run the binary with cwd set so `data/frame.proto` "
      "resolves correctly.");
}

static constexpr const char kControlRelayNexusErr[] =
    "Not registered with Nexus (control heartbeat not acknowledged)";

static std::unordered_map<int, std::chrono::steady_clock::time_point>
    g_control_relay_nexus_pending;
static std::unordered_map<int, std::chrono::steady_clock::time_point>
    g_relay_hb_sent;
static std::unordered_map<int, std::chrono::steady_clock::time_point>
    g_relay_hb_recv;
static constexpr auto kRelayHeartbeatTimeout = std::chrono::seconds(10);

static std::unordered_set<int>
collect_nexus_registered_relay_frame_ids() {
  const auto now = std::chrono::steady_clock::now();
  std::unordered_set<int> out;
  for (auto &[fid, last] : g_relay_hb_recv) {
    if (now - last < kRelayHeartbeatTimeout)
      out.insert(fid);
  }
  return out;
}

static bool control_relay_participates(ComponentState s) {
  return s == ComponentState::ACTIVE || s == ComponentState::ACTIVATING ||
         s == ComponentState::COMP_ERROR;
}

static int control_relay_target_connector_id(const Component &relay) {
  auto it = relay.data.attributes.find("target");
  if (it == relay.data.attributes.end() || !it->second)
    return -1;
  if (it->second->GetType() != AttributeType::INT)
    return -1;
  return std::get<int>(it->second->GetFinalValue());
}

static std::shared_ptr<Component>
find_data_connector_for_control_relay(const Frame &fr, const Component &relay) {
  const int tid = control_relay_target_connector_id(relay);
  if (tid >= 0) {
    for (auto &c : fr.components) {
      if (!c || c->data.id != tid)
        continue;
      if (c->data.get_or<std::string>("type", "") == "Data Connector")
        return c;
    }
    return nullptr;
  }
  for (auto &c : fr.components) {
    if (!c)
      continue;
    if (c->data.get_or<std::string>("type", "") == "Data Connector")
      return c;
  }
  return nullptr;
}

static void ensure_control_relay_link_status_attr(Component &relay) {
  auto &m = relay.data.attributes;
  if (auto it = m.find("link_status"); it != m.end() && it->second)
    return;
  nlohmann::json ins = nlohmann::json::object();
  ins["readonly"] = true;
  m["link_status"] = std::make_shared<Attribute>(
      "Nexus link",
      "Data wire path and Nexus heartbeat registration (engine-maintained).",
      AttributeType::STRING, std::string("-"), AttributeEasing{},
      std::move(ins));
}

static void set_control_relay_link_status(Component &relay, const std::string &v) {
  ensure_control_relay_link_status_attr(relay);
  relay.data.attributes["link_status"]->SetBaseValue(v);
}

static std::string compute_control_relay_link_status(
    const Frame &fr, const Component &relay,
    const std::unordered_set<int> &registered,
    const std::unordered_map<int, std::chrono::steady_clock::time_point> &pending,
    std::chrono::steady_clock::time_point now,
    std::chrono::seconds grace) {
  if (!control_relay_participates(relay.state))
    return "Inactive";
  const auto dc = find_data_connector_for_control_relay(fr, relay);
  if (control_relay_target_connector_id(relay) >= 0 && !dc)
    return "Target link invalid";
  if (!dc)
    return "No data connector";
  if (dc->counterpart_id < 0)
    return "Data wire not connected";
  const int fid = fr.data.id;
  if (registered.find(fid) != registered.end())
    return "Registered with Nexus";
  auto pit = pending.find(fid);
  if (pit == pending.end() || now - pit->second < grace)
    return "Heartbeat sent, awaiting Nexus";
  return "No acknowledgement from Nexus (timeout)";
}

static bool frame_has_active_control_relay(entt::registry &registry, int frame_id) {
  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    if (fr.data.id != frame_id)
      continue;
    for (auto &c : fr.components) {
      if (!c)
        continue;
      if (c->data.get_or<std::string>("type", "") != "Control Relay")
        continue;
      if (c->state == ComponentState::ACTIVE)
        return true;
    }
    return false;
  }
  return false;
}

static void process_relay_heartbeats(entt::registry &registry) {
  const auto now = std::chrono::steady_clock::now();

  for (auto it = g_relay_hb_recv.begin(); it != g_relay_hb_recv.end();) {
    if (now - it->second >= kRelayHeartbeatTimeout ||
        !frame_has_active_control_relay(registry, it->first))
      it = g_relay_hb_recv.erase(it);
    else
      ++it;
  }

  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp) continue;
      if (comp->data.get_or<std::string>("type", "") != "Control Relay")
        continue;
      if (!control_relay_participates(comp->state)) continue;

      int interval_ticks = 60;
      if (auto it = comp->data.attributes.find("heartbeat_interval");
          it != comp->data.attributes.end() && it->second &&
          it->second->GetType() == AttributeType::INT) {
        interval_ticks = std::get<int>(it->second->GetFinalValue());
      }
      auto interval = std::chrono::milliseconds(interval_ticks * 50);

      auto &last_sent = g_relay_hb_sent[fr.data.id];
      if (last_sent.time_since_epoch().count() != 0 && now - last_sent < interval) continue;
      last_sent = now;

      auto dc = find_data_connector_for_control_relay(fr, *comp);
      if (!dc || dc->counterpart_id < 0) continue;

      DataPacket pkt;
      pkt.source = fr.data.id;
      pkt.destination = -1;
      pkt.headers["hb"] = "CONTROL_HEARTBEAT";
      pkt.body = std::to_string(fr.data.id);
      data_link_send_packet(dc, std::move(pkt));
    }
  }

  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp) continue;
      if (comp->data.get_or<std::string>("type", "") != "Nexus") continue;
      if (comp->state != ComponentState::ACTIVE) {
        auto &m = comp->data.attributes;
        if (auto it = m.find("alive_relays"); it != m.end() && it->second)
          it->second->SetBaseValue(std::string{});
        continue;
      }

      std::shared_ptr<Component> dc;
      if (auto it = comp->data.attributes.find("target_data");
          it != comp->data.attributes.end() && it->second &&
          it->second->GetType() == AttributeType::INT) {
        int tid = std::get<int>(it->second->GetFinalValue());
        if (tid >= 0) dc = find_component_by_id(tid);
      }
      if (!dc) {
        for (auto &c : fr.components) {
          if (c && c->data.get_or<std::string>("type", "") == "Data Connector") {
            dc = c;
            break;
          }
        }
      }
      if (!dc) continue;

      constexpr int kDrainCap = 64;
      int n = 0;
      while (n++ < kDrainCap && !dc->data_packet_inbox.empty()) {
        auto pkt = std::move(dc->data_packet_inbox.front());
        dc->data_packet_inbox.pop_front();
        auto it = pkt.headers.find("hb");
        if (it == pkt.headers.end() || it->second != "CONTROL_HEARTBEAT") continue;
        int relay_fid = 0;
        try { relay_fid = std::stoi(pkt.body); } catch (...) { continue; }
        if (relay_fid > 0) g_relay_hb_recv[relay_fid] = now;
      }

      std::vector<std::string> ids;
      ids.reserve(g_relay_hb_recv.size());
      for (auto &[fid, last] : g_relay_hb_recv) {
        (void)last;
        ids.push_back(std::to_string(fid));
      }
      auto &m = comp->data.attributes;
      if (auto it = m.find("alive_relays"); it != m.end() && it->second) {
        it->second->SetBaseValue(ids.empty() ? std::string{} : fmt::format("{}", fmt::join(ids, ",")));
      }
    }
  }
}

static void enforce_control_relay_nexus_registration(
    entt::registry &registry, event_emitter &emitter,
    std::unordered_map<int, std::chrono::steady_clock::time_point> &pending) {
  const auto registered = collect_nexus_registered_relay_frame_ids();
  const auto now = std::chrono::steady_clock::now();
  const auto grace = std::chrono::seconds(4);

  std::unordered_set<int> participating_frames;
  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp)
        continue;
      if (!comp->data.has("control_radius") || comp->data.has("alive_relays"))
        continue;
      if (control_relay_participates(comp->state))
        participating_frames.insert(fr.data.id);
    }
  }

  for (auto it = pending.begin(); it != pending.end();) {
    if (participating_frames.find(it->first) == participating_frames.end())
      it = pending.erase(it);
    else
      ++it;
  }

  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp)
        continue;
      if (!comp->data.has("control_radius") || comp->data.has("alive_relays"))
        continue;

      set_control_relay_link_status(
          *comp, compute_control_relay_link_status(fr, *comp, registered, pending,
                                                    now, grace));

      if (!control_relay_participates(comp->state))
        continue;

      const int fid = fr.data.id;
      if (registered.find(fid) != registered.end()) {
        pending.erase(fid);
        if (comp->state == ComponentState::COMP_ERROR &&
            comp->error == kControlRelayNexusErr) {
          auto prev = comp->state;
          comp->state = ComponentState::ACTIVE;
          comp->error.clear();
          emitter.publish(component_state_changed{
              comp->frame_id,
              comp->data.id,
              comp->data.name,
              static_cast<int>(prev),
              static_cast<int>(comp->state),
              comp->error});
        }
        continue;
      }

      auto pit = pending.find(fid);
      if (pit == pending.end())
        pit = pending.emplace(fid, now).first;
      if (now - pit->second < grace)
        continue;

      if (comp->state != ComponentState::COMP_ERROR ||
          comp->error != kControlRelayNexusErr) {
        auto prev = comp->state;
        comp->state = ComponentState::COMP_ERROR;
        comp->error = kControlRelayNexusErr;
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

// ---- Nexus library heartbeat (Nexus → Advanced Cores) ----

static std::chrono::steady_clock::time_point g_nexus_lib_hb_last;
static constexpr auto kLibraryHeartbeatInterval = std::chrono::milliseconds(3000);
static constexpr auto kLibraryHeartbeatTimeout = std::chrono::seconds(10);

static void set_status_attr(Component& c, const std::string& key, const std::string& val) {
  auto it = c.data.attributes.find(key);
  if (it != c.data.attributes.end() && it->second)
    it->second->SetBaseValue(val);
}

static int g_nexus_lib_receivers = 0; // updated by reception function

static void process_nexus_library_heartbeats(entt::registry &registry) {
  const auto now = std::chrono::steady_clock::now();
  bool should_send = g_nexus_lib_hb_last.time_since_epoch().count() == 0 ||
                     now - g_nexus_lib_hb_last >= kLibraryHeartbeatInterval;

  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp) continue;
      if (comp->data.get_or<std::string>("type", "") != "Nexus") continue;
      if (comp->state != ComponentState::ACTIVE) {
        set_status_attr(*comp, "library_broadcast", "Inactive");
        continue;
      }

      auto lib_it = comp->data.attributes.find("library");
      if (lib_it == comp->data.attributes.end() || !lib_it->second) {
        set_status_attr(*comp, "library_broadcast", "No library code");
        continue;
      }
      auto lib_code = std::get<std::string>(lib_it->second->GetBaseValue());
      if (lib_code.empty()) {
        set_status_attr(*comp, "library_broadcast", "Empty library");
        continue;
      }

      // Find data connector (reuse same target_data logic as relay reception)
      std::shared_ptr<Component> dc;
      if (auto it = comp->data.attributes.find("target_data");
          it != comp->data.attributes.end() && it->second &&
          it->second->GetType() == AttributeType::INT) {
        int tid = std::get<int>(it->second->GetFinalValue());
        if (tid >= 0) dc = find_component_by_id(tid);
      }
      if (!dc) {
        for (auto &c : fr.components) {
          if (c && c->data.get_or<std::string>("type", "") == "Data Connector") {
            dc = c;
            break;
          }
        }
      }
      if (!dc) {
        set_status_attr(*comp, "library_broadcast", "No data connector");
        continue;
      }
      if (dc->counterpart_id < 0) {
        set_status_attr(*comp, "library_broadcast", "Data wire not connected");
        continue;
      }

      if (should_send) {
        DataPacket pkt;
        pkt.source = fr.data.id;
        pkt.destination = -1;
        pkt.headers["hb"] = "LIBRARY_HEARTBEAT";
        pkt.body = lib_code;
        data_link_send_packet(dc, std::move(pkt));
      }

      int n = g_nexus_lib_receivers;
      set_status_attr(*comp, "library_broadcast",
                      n > 0 ? fmt::format("Broadcasting to {} core{}", n, n == 1 ? "" : "s")
                             : "Broadcasting (no receivers)");
    }
  }
  if (should_send)
    g_nexus_lib_hb_last = now;
}

static void process_advanced_core_library_reception(entt::registry &registry) {
  const auto now = std::chrono::steady_clock::now();

  // Expire stale entries
  for (auto it = g_core_library.begin(); it != g_core_library.end();) {
    if (now - it->second.second >= kLibraryHeartbeatTimeout)
      it = g_core_library.erase(it);
    else
      ++it;
  }

  int receiver_count = 0;

  for (auto e : registry.view<Frame>()) {
    auto &fr = registry.get<Frame>(e);
    for (auto &comp : fr.components) {
      if (!comp) continue;
      if (comp->data.get_or<std::string>("type", "") != "Core") continue;
      if (!comp->data.has("memory")) continue; // Advanced Core only

      if (comp->state != ComponentState::ACTIVE) {
        set_status_attr(*comp, "library_status", "Inactive");
        continue;
      }

      // Find a Data Connector on this frame
      std::shared_ptr<Component> dc;
      for (auto &c : fr.components) {
        if (c && c->data.get_or<std::string>("type", "") == "Data Connector") {
          dc = c;
          break;
        }
      }
      if (!dc) {
        set_status_attr(*comp, "library_status", "No data connector");
        continue;
      }

      constexpr int kDrainCap = 64;
      int n = 0;
      while (n++ < kDrainCap && !dc->data_packet_inbox.empty()) {
        auto &front = dc->data_packet_inbox.front();
        auto hb_it = front.headers.find("hb");
        if (hb_it != front.headers.end() && hb_it->second == "LIBRARY_HEARTBEAT") {
          g_core_library[comp->data.id] = {front.body, now};
          dc->data_packet_inbox.pop_front();
        } else {
          break; // Don't consume non-library packets
        }
      }

      // Set status based on whether we have a fresh library
      auto lib_it = g_core_library.find(comp->data.id);
      if (lib_it != g_core_library.end()) {
        receiver_count++;
        set_status_attr(*comp, "library_status", "Library active");
      } else {
        set_status_attr(*comp, "library_status", "No heartbeat");
      }
    }
  }

  g_nexus_lib_receivers = receiver_count;
}

} // namespace

GameManager::GameManager() {
  log.is_debug = entt::monostate<"debug"_hs>{};
  startJob = std::make_shared<Job>("GameManager start",
                                   std::bind(&GameManager::start, this));
  entt::locator<std::mutex *>::emplace();
  entt::locator<Oracle>::emplace();
  entt::locator<FrameWorld>::emplace();
}

GameManager::~GameManager() {}

void GameManager::loadData(bool forceFromInit) {
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
  auto init_state_files =
      lua["settings"]["init_states"].get<std::vector<std::string>>();

  auto rebuildFromInit = [&]() {
    log.info("Creating state from init");
    auto init_state = std::make_shared<State>();
    loader.load<State>(*init_state, init_state_files);
    log.var("Init State", init_state->registry.storage<hf::meta>().size());
    current_state.registry.clear();
    current_state.stores.clear();
    auto store = current_state.create("current", state_path);
    store->initEmpty();
    EnttTools::copyRegistry(init_state->registry, store->registry);
    current_state.add(store);
    log.var("State Path", current_state.stores.front()->path.string());
  };

  if (forceFromInit || !fs::exists(state_path)) {
    log.info(forceFromInit ? "Forced new game from init" : "No save file; creating from init");
    rebuildFromInit();
  } else {
    log.info("Loading current state");
    loader.load<State>(current_state, {state_path.string()});
    if (current_state.registry.storage<hf::meta>().size() == 0) {
      log.warn("Save unusable (corrupt or incompatible with this build); reinitializing from init");
      try {
        fs::path bak = state_path;
        bak += ".corrupt";
        if (fs::exists(bak))
          fs::remove(bak);
        fs::rename(state_path, bak);
        log.warn("Renamed broken save to {}", bak.string());
      } catch (const std::exception &e) {
        log.warn("Could not rename broken save: {}", e.what());
      }
      rebuildFromInit();
    } else {
      log.var("Current State", current_state.registry.storage<hf::meta>().size());
    }
  }
  if (research) {
    research->loadUnlocked(PATH / "save");
  }
  log.stop("Loading State");
  startJob->progress += 5;

  // Ensure directories exist for save slots, backups, and init states
  {
    std::error_code ec;
    fs::create_directories(PATH / "save" / "slots", ec);
    fs::create_directories(PATH / "save" / "backup", ec);
    fs::create_directories(PATH / "data" / "init", ec);
  }

  log.start("Loading Patch Types");
  auto& patch_loader = entt::locator<PatchLoader>::emplace();
  fs::path patches_path = PATH / "scripts" / "patches";
  patch_loader.load_patches(patches_path.string(), lua);
  log.var("Patch Types", patch_loader.get_patch_types().size());
  log.stop("Loading Patch Types");

  WellKnownEntities wk;
  for (auto e : current_state.registry.view<Environment>()) {
    wk.environment = e;
    break;
  }
  for (auto e : current_state.registry.view<SpendablePool>()) {
    wk.economy = e;
    break;
  }
  for (auto e : current_state.registry.view<hf::meta>()) {
    auto &meta = current_state.registry.get<hf::meta>(e);
    if (meta.name == "Frames") wk.frames_folder = e;
    if (meta.name == "Connections") wk.connections_folder = e;
    if (meta.name == "Patches") wk.patches_folder = e;
  }
  ensureEconomyEntity(current_state.registry, wk);
  if (!current_state.stores.empty()) {
    auto &st = current_state.stores.front();
    if (st->file_version < 3 && wk.economy != entt::null &&
        current_state.registry.valid(wk.economy) &&
        current_state.registry.all_of<SpendablePool>(wk.economy)) {
      mergeLegacySpendablesJsonInto(
          current_state.registry.get<SpendablePool>(wk.economy).amounts);
    }
  }
  entt::locator<WellKnownEntities>::emplace(wk);

  Metadata::reconcileGlobalIdCounter(current_state.registry);
  stripInvalidFrameComponents(current_state.registry);

  if (exec) {
    exec->invalidateAllScripts();
  }

  g_control_relay_nexus_pending.clear();
  g_relay_hb_sent.clear();
  g_relay_hb_recv.clear();
  rpc::reset_control_zones_update_cache();

  started = true;

  if (exec) {
    refresh_component_apis(*exec);
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
  {
    bool have_env_proto = false;
    for (auto e : prototypes.registry.view<hf::meta>()) {
      if (prototypes.registry.get<hf::meta>(e).id == "ENV") {
        have_env_proto = true;
        break;
      }
    }
    if (have_env_proto) {
      loader.save(prototypes);
    } else {
      log.warn(
          "Skipping save of frame.proto — prototypes did not load (would overwrite template data)");
    }
  }

  auto &state = entt::locator<State>::value();
  if (!state.stores.empty()) {
    // Use saveStateToFile to write the live registry (not stale store data)
    loader.saveStateToFile(state, state.stores.front()->path.string());
  }
  if (research) {
    fs::path PATH = entt::monostate<"path"_hs>{};
    research->saveUnlocked(PATH / "save");
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

bool GameManager::autoBackup() {
  auto state_path = currentStatePath();

  if (!fs::exists(state_path)) return false;

  auto backup_dir = state_path.parent_path().parent_path() / "backup";
  std::error_code ec;
  fs::create_directories(backup_dir, ec);

  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&t), "%Y%m%dT%H%M%SZ");
  auto backup_path = backup_dir / ("backup_" + oss.str() + ".state");

  fs::copy_file(state_path, backup_path, fs::copy_options::overwrite_existing, ec);
  if (ec) {
    log.warn("autoBackup: failed to copy {} → {}: {}", state_path.string(), backup_path.string(), ec.message());
    return false;
  }
  return true;
}

fs::path GameManager::currentStatePath() const {
  auto &lua = entt::locator<sol::state>::value();
  fs::path PATH = entt::monostate<"path"_hs>{};
  return PATH / fs::path(lua["settings"]["current_state"].get<std::string>());
}

void GameManager::ensureEconomyEntity(entt::registry &reg, WellKnownEntities &wk) {
  if (wk.economy != entt::null && reg.valid(wk.economy) &&
      reg.all_of<SpendablePool>(wk.economy)) {
    return;
  }
  wk.economy = entt::null;
  for (auto e : reg.view<SpendablePool>()) {
    wk.economy = e;
    break;
  }
  if (wk.economy != entt::null && reg.valid(wk.economy)) {
    return;
  }
  const entt::entity e = reg.create();
  hf::meta m;
  m.name = "Economy";
  m.id = "ECONOMY";
  reg.emplace<hf::meta>(e, m);
  SpendablePool sp;
  auto &lua = entt::locator<sol::state>::value();
  sol::optional<std::map<std::string, int64_t>> pool_cfg =
      lua["settings"]["spendable_pool"];
  if (pool_cfg) sp.amounts = *pool_cfg;
  reg.emplace<SpendablePool>(e, std::move(sp));
  wk.economy = e;
}

std::map<std::string, int64_t> GameManager::spendablePool() const {
  auto &state = entt::locator<State>::value();
  const auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.economy == entt::null || !state.registry.valid(wk.economy) ||
      !state.registry.all_of<SpendablePool>(wk.economy)) {
    return {};
  }
  return state.registry.get<SpendablePool>(wk.economy).amounts;
}

void GameManager::ensureSpendableKeysFromItems() {
  if (!items || !items->loader) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(updateMutex);
  auto &state = entt::locator<State>::value();
  auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.economy == entt::null || !state.registry.valid(wk.economy) ||
      !state.registry.all_of<SpendablePool>(wk.economy)) {
    return;
  }
  auto &amounts = state.registry.get<SpendablePool>(wk.economy).amounts;
  for (const auto &[name, def] : items->loader->get_items()) {
    if (def.spendable && amounts.count(name) == 0) {
      amounts[name] = 0;
    }
  }
}

void GameManager::emitSpendablePool() {
  auto &emitter = entt::locator<event_emitter>::value();
  spendable_pool_changed_event ev;
  ev.amounts = spendablePool();
  emitter.publish(ev);
}

void GameManager::addSpendable(const std::string &name, int64_t delta) {
  std::lock_guard<std::recursive_mutex> lock(updateMutex);
  auto &state = entt::locator<State>::value();
  auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.economy == entt::null || !state.registry.valid(wk.economy) ||
      !state.registry.all_of<SpendablePool>(wk.economy)) {
    return;
  }
  state.registry.get<SpendablePool>(wk.economy).amounts[name] += delta;
  emitSpendablePool();
}

bool GameManager::tryConsumeSpendable(const std::map<std::string, int> &cost,
                                      std::string &err) {
  auto &state = entt::locator<State>::value();
  auto &wk = entt::locator<WellKnownEntities>::value();
  if (wk.economy == entt::null || !state.registry.valid(wk.economy) ||
      !state.registry.all_of<SpendablePool>(wk.economy)) {
    err = "Economy not available";
    return false;
  }
  auto &amounts = state.registry.get<SpendablePool>(wk.economy).amounts;
  for (const auto &[k, v] : cost) {
    if (v <= 0)
      continue;
    if (amounts[k] < v) {
      err = "Not enough " + k;
      return false;
    }
  }
  bool changed = false;
  for (const auto &[k, v] : cost) {
    if (v > 0) {
      amounts[k] -= v;
      changed = true;
    }
  }
  if (changed)
    emitSpendablePool();
  return true;
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

  for (auto cn : spec["components"].get_or<std::vector<std::string>>({})) {
    ComponentSize sz = warlock::component_script_size(lua, exec->getScript(cn));
    if (!frame_has_slot_for_component_size(frame, sz)) {
      throw std::runtime_error("Blueprint exceeds component size slots for this frame size");
    }
    auto component = create_component_from_lua(exec->getState(frame.data.id),
                                               exec->getScript(cn));
    if (cn == "Core" || cn == "Main Core") {
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
                 AttributeType::FLOAT, -1000.0f, AttributeEasing(),
                 nlohmann::json{{"precision", 1}});
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
  log.setParent(nullptr);
  log.setAsync(true);

  // Auto-generate map for new games (no patches yet)
  {
    auto &current_state_pre = entt::locator<State>::value();
    if (current_state_pre.registry.view<ResourcePatch>().size() == 0) {
      log.info("No patches found — generating initial map");
      MapGenLoader mapgen_loader;
      mapgen_loader.load((PATH / "scripts" / "mapgen").string(), lua);
      MapGenerator gen;
      auto result = gen.generate(mapgen_loader.config(), mapgen_loader.biomes(),
                                  mapgen_loader.features(), current_state_pre.registry, lua);
      log.var("Map seed", std::to_string(result.seed_used));
      log.var("Patches placed", result.patches_placed);
    }
  }

  auto label = "Location generation";
  log.start(label);
  auto &current_state = entt::locator<State>::value();

  emitter.publish(ready_event{"game_manager"});
  log.stop(label);

  systems.push_back(std::make_shared<TweeningSystem>());
  systems.push_back(std::make_shared<PowerSystem>());
  systems.push_back(std::make_shared<ThermalSystem>());
  systems.push_back(std::make_shared<EnvironmentSystem>());
  systems.push_back(std::make_shared<WirelessConnectionSystem>());
  exec = std::make_shared<CodeExecutionSystem>();
  refresh_component_apis(*exec);
  systems.push_back(exec);
  items = std::make_shared<ItemsSystem>();
  systems.push_back(items);

  research = std::make_shared<ResearchManager>();
  research->load(lua, PATH / "scripts/research");

  ensureSpendableKeysFromItems();
  emitSpendablePool();

  for (auto &c : exec->sources) {
    components.push_back(c.first);
  }

  emitter.connect<exec_lua_function>([=, this](const auto &e, const auto &em) {
    exec->executeCoreFunction(e.component, e.function_name);
  });

  emitter.connect<component_state_changed>(
    std::function<void(component_state_changed&, const event_emitter&)>(
      [this](component_state_changed& event, const event_emitter&) {
        if (exec) {
          exec->dispatchStateChange(event.frame_id, event.component_id,
                                    event.component_name, event.prev_state,
                                    event.new_state, event.reason);
        }
      }
    )
  );

  emitter.connect<add_component>([this](const auto &e, const auto &em) {
    auto &lua_st = entt::locator<sol::state>::value();
    ComponentSize sz =
        warlock::component_script_size(lua_st, exec->getScript(e.component_name));
    if (!frame_has_slot_for_component_size(e.frame, sz)) {
      throw std::runtime_error("Frame has no free slot for this component size");
    }
    auto component = create_component_from_lua(
        exec->getState(e.frame.data.id), exec->getScript(e.component_name));
    e.frame.addComponent(component);
  });

  WellKnownEntities wk_loaded;
  for (auto e : current_state.registry.view<Environment>()) {
    wk_loaded.environment = e;
    break;
  }
  for (auto e : current_state.registry.view<SpendablePool>()) {
    wk_loaded.economy = e;
    break;
  }
  for (auto e : current_state.registry.view<hf::meta>()) {
    auto &meta = current_state.registry.get<hf::meta>(e);
    if (meta.name == "Frames") wk_loaded.frames_folder = e;
    if (meta.name == "Connections") wk_loaded.connections_folder = e;
    if (meta.name == "Patches") wk_loaded.patches_folder = e;
  }
  const bool persisted_world_ok =
      wk_loaded.environment != entt::null &&
      current_state.registry.valid(wk_loaded.environment) &&
      wk_loaded.frames_folder != entt::null &&
      current_state.registry.valid(wk_loaded.frames_folder);

  if (persisted_world_ok) {
    ensureEconomyEntity(current_state.registry, wk_loaded);
    entt::locator<WellKnownEntities>::emplace(wk_loaded);
    log.setAsync(false);
    log.setParent(p);
    started = true;
    return;
  }

  if (current_state.registry.template storage<entt::entity>().size() > 0) {
    current_state.registry.clear();
  }

  require_bootstrap_prototypes();

  WellKnownEntities wk;
  wk.environment = EnttTools::createEntityFromPrototype("ENV", current_state.registry);
  wk.frames_folder = EnttTools::createEntityFromPrototype("FOLDER", current_state.registry,
                                       "Frames");
  wk.connections_folder = EnttTools::createEntityFromPrototype("FOLDER", current_state.registry,
                                       "Connections");
  wk.patches_folder = EnttTools::createEntityFromPrototype("FOLDER", current_state.registry,
                                       "Patches");
  ensureEconomyEntity(current_state.registry, wk);
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
  nlohmann::json push_control_zones;
  bool do_push_control_zones = false;
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
              // "Storage" requirement is satisfied by any component with actual storage capacity
              if (needed == "Storage" && other->storage != nullptr) {
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

    {
      auto &emitter = entt::locator<event_emitter>::value();
      process_relay_heartbeats(current_state.registry);
      enforce_control_relay_nexus_registration(
          current_state.registry, emitter, g_control_relay_nexus_pending);
      process_nexus_library_heartbeats(current_state.registry);
      process_advanced_core_library_reception(current_state.registry);
    }

    if (entt::locator<rpc::Server>::has_value()) {
      auto &rpcServer = entt::locator<rpc::Server>::value();
      if (rpcServer.clientCount() > 0 &&
          rpc::take_control_zones_if_changed(current_state.registry,
                                              &push_control_zones)) {
        do_push_control_zones = true;
      }
    }

    updateMutex.unlock();
  }

  if (do_push_control_zones && entt::locator<rpc::Server>::has_value()) {
    auto &rpcServer = entt::locator<rpc::Server>::value();
    if (rpcServer.clientCount() > 0) {
      rpcServer.broadcast("event.state_update",
                          {{"control_zones", push_control_zones}});
    }
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

        nlohmann::json control_zones = rpc::computeControlZones(registry);
        rpc::remember_control_zones_json(control_zones);
        rpcServer.broadcast("event.state_update", {
          {"tick", tick_count_},
          {"frames", frames},
          {"connections", connections},
          {"control_zones", std::move(control_zones)}
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

