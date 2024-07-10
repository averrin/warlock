#pragma once
#include <memory>
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
  EntityTreeEditor(std::string name) : name(name) {}

  template <typename ContainerType, typename... T> void render();

  // std::map<std::string, std::string> cache = {};
  // std::map<std::string, float> cache_f = {};
};
