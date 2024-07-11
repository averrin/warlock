#pragma once
#include <game/registry_store.hpp>
#include <map>
#include <memory>
#include <string>
#include <utils/entt.hpp>
#include <vector>

typedef std::vector<entt::entity> entity_list;
struct tree_node {
  entity_list entities;
  std::map<std::string, std::shared_ptr<tree_node>> children;
};

class EntityTreeEditor {
public:
  std::string name;
  EntityTreeEditor(std::string name);

  template <typename ContainerType, typename... T> void render();
  void render();

  void drawEntityInfo(std::shared_ptr<RegistryStore> data, entt::entity e);

  // std::map<std::string, std::string> cache = {};
  // std::map<std::string, float> cache_f = {};
};
