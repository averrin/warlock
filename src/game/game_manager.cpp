#include <chrono>
#include <filesystem>
#include <game/game_manager.hpp>
#include <mutex>
#include <thread>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
namespace fs = std::filesystem;
using namespace std::this_thread;     // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <game/meta_data.hpp>
#include <game/prototypes.hpp>
#include <game/state.hpp>
#include <utils/data/loader.hpp>

#include <algorithm> // For std::ranges::transform
#include <fmt/ranges.h>
#include <game/components/frame.hpp>
#include <game/systems/power.hpp>
#include <game/systems/tweening.hpp>
#include <game/systems/environment.hpp>
#include <game/systems/code_execution.hpp>
#include <game/systems/thermal.hpp>
#include <ranges> // For ranges

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

entt::entity createEntityFromPrototype(std::string protoID,
                                       entt::registry &reg) {
  auto &prototypes = entt::locator<Prototypes>::value();
  for (auto entity : prototypes.registry.view<hf::meta>()) {
    auto meta = prototypes.registry.get<hf::meta>(entity);
    if (meta.id == protoID) {
      auto copy =
          RegistryContainer::copyEntity(entity, prototypes.registry, reg);
      fmt::print("Proto: {}.{} -> {}\n", meta.id, (int)entity, (int)copy);
      return copy;
    }
  }
  throw std::runtime_error("Prototype not found");
}

GameManager::GameManager() {
  log.is_debug = entt::monostate<"debug"_hs>{};
  startJob = std::make_shared<Job>("GameManager start",
                                   std::bind(&GameManager::start, this));
  entt::locator<std::mutex *>::emplace();
}

GameManager::~GameManager() {}

void GameManager::loadData() {
  auto p = log.parent;
  log.setParent(nullptr);
  log.setAsync(true);
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
    RegistryContainer::copyRegistry(init_state->registry, store->registry);
    current_state.add(store);
    log.var("State Path", current_state.stores.front()->path.string());
  } else {
    log.info("Loading current state");
    loader.load<State>(current_state, {state_path.string()});
    log.var("Current State", current_state.registry.storage<hf::meta>().size());
  }
  log.stop("Loading State");
  startJob->progress += 5;

  started = true;

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
  // fmt::print("test: {}\n", metaData.mapFeatures["DUNGEON-"]["test"]);
  loader.save(metaData);
  auto &prototypes = entt::locator<Prototypes>::value();
  loader.save(prototypes);

  auto &state = entt::locator<State>::value();
  loader.save(state);
  log.stop(label);

  log.setAsync(false);
  log.setParent(p);
}

void GameManager::init(LibLog::Logger parentLog) {
  log.setParent(&parentLog);
  auto label = "Initializing GameManager";
  log.start(label);
  auto &emitter = entt::locator<event_emitter>::value();
  auto &lua = entt::locator<sol::state>::value();

  auto &registry = entt::locator<entt::registry>::emplace();
  registry.on_construct<hf::script>().connect<&GameManager::initScript>(this);
  registry.on_destroy<hf::script>().connect<&GameManager::releaseScript>(this);

  lua.new_usertype<GameManager>("GameManager", "new", sol::no_constructor);

  lua.set("gm", this);

  lastUpdate = hr_clock::now();
  emitter.publish(init_event{"game_manager"});
  log.stop(label);
}

entt::entity GameManager::addConnection(int source, int target) {
  auto &current_state = entt::locator<State>::value();
  auto e = createEntityFromPrototype("CONNECTION", current_state.registry);
  auto &c = current_state.registry.get<Connection>(e);
  auto &meta = current_state.registry.get<hf::meta>(e);
  c.data.id = Metadata::newId();

  Attribute is_bidirection("Is Bidirectional",
                           "Is the connection bidirectional",
                           AttributeType::BOOL, true);
  c.data.attributes["bidirectional"] =
      std::make_shared<Attribute>(is_bidirection);

  c.source = source;
  c.target = target;
  current_state.registry.emplace_or_replace<Connection>(e, c);
  auto name = fmt::format("{} -> {}", source ,target);

  meta.name = name;
  meta.id = fmt::format("CONNECTION-{}", (int)e);
  current_state.registry.emplace_or_replace<hf::meta>(e, meta);
  return e;
}

entt::entity GameManager::addFrame(std::string_view name) {
  auto &current_state = entt::locator<State>::value();
  auto e = createEntityFromPrototype("FRAME", current_state.registry);
  auto &frame = current_state.registry.get<Frame>(e);
  auto &meta = current_state.registry.get<hf::meta>(e);
  frame.data.id = Metadata::newId();
  frame.data.name = name;

  Attribute temp("Temperature",
                           "Total frame temperature in Celsius",
                           AttributeType::FLOAT, -1000.0f);
  frame.data.attributes["temp"] =
      std::make_shared<Attribute>(temp);

  current_state.registry.emplace_or_replace<Frame>(e, frame);

  meta.name = fmt::format("{} [{}]", name, frame.data.id);
  meta.id = fmt::format("FRAME-{}", (int)e);
  current_state.registry.emplace_or_replace<hf::meta>(e, meta);
  return e;
}

void GameManager::start() {
  auto p = log.parent;
  auto &emitter = entt::locator<event_emitter>::value();
  auto &lua = entt::locator<sol::state>::value();

  loadData();
  saveData();

  log.setParent(nullptr);
  log.setAsync(true);
  auto label = "Location generation";
  log.start(label);
  auto &current_state = entt::locator<State>::value();

  /*
  auto e = current_state.registry.create();
  current_state.registry.emplace<hf::script>(
      e, hf::script{"scripts/entities/test.lua"});
      */
  // auto &script = current_state.registry.get<hf::script>(e);
  // script.self["counter"] = 24;

  /*
  std::this_thread::sleep_for(2s);
  startJob->progress += 25;
  emitter.publish(job_update_event{startJob});
  std::this_thread::sleep_for(2s);
  startJob->progress += 25;
  */
  // emitter.publish(job_update_event{startJob});
  // std::this_thread::sleep_for(2s);

  emitter.publish(ready_event{"game_manager"});
  log.stop(label);

  systems.push_back(std::make_shared<TweeningSystem>());
  systems.push_back(std::make_shared<PowerSystem>());
  systems.push_back(std::make_shared<ThermalSystem>());
  systems.push_back(std::make_shared<EnvironmentSystem>());
  auto exec = std::make_shared<CodeExecutionSystem>();
  systems.push_back(exec);
  auto ces_id = 4;

  for (auto &c : exec->sources) {
    components.push_back(c.first);
  }

  emitter.connect<exec_lua_function>([=](const auto &e, const auto &em) {
    std::dynamic_pointer_cast<CodeExecutionSystem>(systems[ces_id])->executeCoreFunction(e.component, e.function_name);
  });

  emitter.connect<add_component>([=](const auto &e, const auto &em) {
    auto component = create_component_from_lua(
      std::dynamic_pointer_cast<CodeExecutionSystem>(systems[ces_id])->getState(e.frame.data.id),
      std::dynamic_pointer_cast<CodeExecutionSystem>(systems[ces_id])->getScript(e.component_name));
    e.frame.addComponent(component);
  });

  if (current_state.registry.template storage<entt::entity>().size() > 0) {
    log.setAsync(false);
    log.setParent(p);
    started = true;
    return;
  }

  auto env = createEntityFromPrototype("ENV", current_state.registry);

  auto component_names = std::vector<std::string_view>{
      "Nexus",      "Generator SIN",   "Battery",   "Consumer 1",
      "Consumer 2", "Generator SAW", "Consumer 3"};
  auto frames = std::vector<entt::entity>{};
  auto frame_ids = std::map<entt::entity, int>{};

  auto makeFrame = [&](std::string_view name) {
    auto e = addFrame(name);
    auto &frame = current_state.registry.get<Frame>(e);
    auto core = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("core.lua"));
    frame.addComponent(core);

    auto connector = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("power_wire_connector.lua"));
    frame.addComponent(connector);

    frame_ids[e] = frame.data.id;
    auto generator = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("generator.lua"));
    if (frame.data.name.starts_with("Nexus")) {
      auto nexus = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("nexus.lua"));
      frame.addComponent(nexus);
    } else if (frame.data.name.starts_with("Generator")) {
      frame.addComponent(generator);
    }

    if (frame.data.name.ends_with("SAW")) {
      generator->data.attributes["production"]->SetEasing({AttributeEasingType::SAW, 0.7f, 2000});
    } else if (frame.data.name.starts_with("Battery")) {
      auto charger = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("charger.lua"));
      auto battery = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("battery.lua"));
      frame.addComponent(battery);
      frame.addComponent(charger);
      auto meter = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("power_meter.lua"));
      frame.addComponent(meter);
    } else {
      auto consumer = create_component_from_lua(exec->getState(frame.data.id), exec->getScript("consumer.lua"));
      frame.addComponent(consumer);
    }

    for (auto &c : frame.components) {
      // c->data.id = Metadata::newId();
      log.var("    Component", c->data.name);
      log.var("    Component ID", c->data.id);
    }

    return e;
  };

  std::ranges::transform(component_names, std::back_inserter(frames),
                         makeFrame);

  addConnection(frame_ids[frames[0]], frame_ids[frames[1]]);
  addConnection(frame_ids[frames[1]], frame_ids[frames[2]]);
  addConnection(frame_ids[frames[2]], frame_ids[frames[3]]);
  addConnection(frame_ids[frames[2]], frame_ids[frames[4]]);
  addConnection(frame_ids[frames[5]], frame_ids[frames[6]]);

  log.setAsync(false);
  log.setParent(p);
  started = true;
}

void GameManager::serve() {
  if (!started)
    return;

  updateMutex.lock();
  auto &current_state = entt::locator<State>::value();
  for (auto &system : systems) {
    const std::chrono::duration<double, std::milli> delta =
        hr_clock::now() - lastUpdate;
    system->update(delta);
  }
  /*
   * Make it system
  for (auto &e : current_state.registry.view<hf::script>()) {
    auto &script = current_state.registry.get<hf::script>(e);
    if (!script.enabled)
      continue;
    const std::chrono::duration<double, std::milli> delta =
        hr_clock::now() - lastUpdate;
    script.handlers.update(script.self, delta.count());
  }
*/
  lastUpdate = hr_clock::now();
  updateMutex.unlock();
}

void GameManager::initScript(entt::registry &registry, entt::entity entity) {
  fs::path PATH = entt::monostate<"path"_hs>{};
  registry.patch<hf::script>(entity, [&](auto &script) {
    auto &lua = entt::locator<sol::state>::value();
    log.var("Script", script.path);
    script.self = lua.load_file(PATH / script.path).call();
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
