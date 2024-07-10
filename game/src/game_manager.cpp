#include <chrono>
#include <filesystem>
#include <game_manager.hpp>
#include <mutex>
#include <thread>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>
namespace fs = std::filesystem;
using namespace std::this_thread;     // sleep_for, sleep_until
using namespace std::chrono_literals; // ns, us, ms, s, h, etc.

#include <meta_data.hpp>
#include <prototypes.hpp>
#include <utils/data/loader.hpp>

GameManager::GameManager() {
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
  loader.load<MetaData, MetaDataStore>(metaData, files);
  log.info("Loaded probabilities: {}", metaData.probability.size());
  log.info("Loaded mapFeatures: {}", metaData.mapFeatures.size());
  log.stop("Loading MetaData");
  startJob->progress += 5;

  log.start("Loading Prototypes");
  auto &prototypes = entt::locator<Prototypes>::emplace();
  auto proto_files =
      lua["settings"]["proto_files"].get<std::vector<std::string>>();
  loader.load<Prototypes, RegistryStore>(prototypes, proto_files);
  log.info("Loaded prototypes: {}",
           prototypes.registry.storage<hf::meta>().size());
  log.stop("Loading Prototypes");
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

void GameManager::start() {
  auto p = log.parent;
  auto &emitter = entt::locator<event_emitter>::value();
  auto &registry = entt::locator<entt::registry>::value();
  auto &lua = entt::locator<sol::state>::value();

  loadData();
  saveData();

  log.setParent(nullptr);
  log.setAsync(true);
  auto label = "Location generation";
  log.start(label);

  auto e = registry.create();
  registry.emplace<hf::script>(e, hf::script{"scripts/entities/test.lua"});
  // auto &script = registry.get<hf::script>(e);
  // script.self["counter"] = 24;

  std::this_thread::sleep_for(2s);
  startJob->progress += 25;
  emitter.publish(job_update_event{startJob});
  std::this_thread::sleep_for(2s);
  startJob->progress += 25;
  // emitter.publish(job_update_event{startJob});
  // std::this_thread::sleep_for(2s);

  started = true;
  log.stop(label);

  log.setAsync(false);
  log.setParent(p);
}

void GameManager::serve() {
  if (!started)
    return;
  auto &registry = entt::locator<entt::registry>::value();
  for (auto &e : registry.view<hf::script>()) {
    auto &script = registry.get<hf::script>(e);
    if (!script.enabled)
      continue;
    const std::chrono::duration<double, std::milli> delta =
        hr_clock::now() - lastUpdate;
    script.handlers.update(script.self, delta.count());
  }
  lastUpdate = hr_clock::now();
}

void GameManager::initScript(entt::registry &registry, entt::entity entity) {
  fs::path PATH = entt::monostate<"path"_hs>{};
  registry.patch<hf::script>(entity, [&](auto &script) {
    auto &lua = entt::locator<sol::state>::value();
    log.debug("Loading script: {}", (PATH / script.path).string());
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
