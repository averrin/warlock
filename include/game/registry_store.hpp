#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <filesystem>
#include <game/components.hpp>
#include <map>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include <game/specs/light.hpp>
#include <utils/data/store.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

class RegistryStore : public Store {
  friend class cereal::access;
  template <class Archive> void load(Archive &ar) {
    ar(cereal::base_class<Store>(this));

    entt::snapshot_loader{registry}
        .get<entt::entity>(ar)
        .template get<hf::meta>(ar)
        .template get<hf::visible>(ar)
        .template get<hf::ineditor>(ar)
        .template get<hf::glow>(ar)
        .template get<hf::renderable>(ar)
        .template get<hf::wall>(ar)
        .template get<hf::tags>(ar)
        .template get<hf::player>(ar)
        .template get<hf::vision>(ar)
        .template get<hf::obstacle>(ar)
        .template get<hf::creature>(ar)
        .template get<hf::script>(ar)
        .template get<entt::tag<"proto"_hs>>(ar)
        .orphans();
  };
  template <class Archive> void save(Archive &ar) const {
    ar(cereal::base_class<Store>(this));
    // TODO: make view configurable
    // auto view = registry.view<entt::tag<"proto"_hs>, hf::meta>();
    auto view = registry.view<entt::entity>();
    entt::snapshot{registry}
        .get<entt::entity>(ar)
        .template get<hf::meta>(ar, view.begin(), view.end())
        .template get<hf::visible>(ar, view.begin(), view.end())
        .template get<hf::ineditor>(ar, view.begin(), view.end())
        .template get<hf::glow>(ar, view.begin(), view.end())
        .template get<hf::renderable>(ar, view.begin(), view.end())
        .template get<hf::wall>(ar, view.begin(), view.end())
        .template get<hf::tags>(ar, view.begin(), view.end())
        .template get<hf::player>(ar, view.begin(), view.end())
        .template get<hf::vision>(ar, view.begin(), view.end())
        .template get<hf::obstacle>(ar, view.begin(), view.end())
        .template get<hf::creature>(ar, view.begin(), view.end())
        .template get<hf::script>(ar, view.begin(), view.end())
        .template get<entt::tag<"proto"_hs>>(ar, view.begin(), view.end());
  };

public:
  entt::registry registry;

  using Store::Store;
};
