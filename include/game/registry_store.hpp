#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cstdint>
#include <filesystem>
#include <game/components.hpp>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;

#include <game/component_registry.hpp>
#include <game/components/frame.hpp>
#include <liblog/liblog.hpp>
#include <utils/data/field_archive.hpp>
#include <utils/data/store.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

// ---------------------------------------------------------------------------
// Dispatch helpers: save/load a component via FieldArchive.
//
// For normal components, calls comp.field_save(foa) / comp.field_load(fia).
// For tag components (where registry.get<T>() returns void), writes an empty
// FieldOutputArchive (zero fields) and reads nothing on load — the entity's
// membership in the view is enough to restore the tag.
// ---------------------------------------------------------------------------
namespace detail {

// Trait: is the component an empty type?
// EnTT applies Empty Type Optimization to all empty structs, making
// registry.get<T>() return void. We must handle these the same as tags.
template <typename T>
struct is_empty_component : std::bool_constant<std::is_empty_v<T>> {};

// Save: normal component
template <typename T>
std::enable_if_t<!is_empty_component<T>::value>
save_component(const entt::registry &reg, entt::entity entity,
               FieldOutputArchive &foa) {
  reg.get<T>(entity).field_save(foa);
}

// Save: tag component (no data)
template <typename T>
std::enable_if_t<is_empty_component<T>::value>
save_component(const entt::registry &, entt::entity, FieldOutputArchive &) {}

// Load: normal component
template <typename T>
std::enable_if_t<!is_empty_component<T>::value>
load_component(T &comp, FieldInputArchive &fia) {
  comp.field_load(fia);
}

// Load: tag component (no data to read)
template <typename T>
std::enable_if_t<is_empty_component<T>::value>
load_component(T &, FieldInputArchive &) {}

// Emplace: normal component — construct default, populate fields, emplace
template <typename T>
std::enable_if_t<!is_empty_component<T>::value>
emplace_component(entt::registry &reg, entt::entity entity,
                  FieldInputArchive &fia) {
  T comp{};
  load_component(comp, fia);
  (void)reg.emplace_or_replace<T>(entity, std::move(comp));
}

// Emplace: tag component — just mark the entity as having the tag
template <typename T>
std::enable_if_t<is_empty_component<T>::value>
emplace_component(entt::registry &reg, entt::entity entity,
                  FieldInputArchive &) {
  if (!reg.all_of<T>(entity)) {
    (void)reg.emplace<T>(entity);
  }
}

} // namespace detail

// ---------------------------------------------------------------------------
// RegistryStore
// ---------------------------------------------------------------------------
class RegistryStore : public Store {
  friend class cereal::access;

  LibLog::Logger log = LibLog::Logger(fmt::color::orange, "RegStr");

  // -------------------------------------------------------------------------
  // V2 save: manifest-based, named-field format.
  // For each component type in COMPONENT_LIST:
  //   - serialize all entities that have it into a size-prefixed block
  //   - each entity's component data is a named-field map (FieldOutputArchive)
  // -------------------------------------------------------------------------
  template <class Archive> void save_v2(Archive &ar) const {
    constexpr uint32_t component_count = [] {
      uint32_t n = 0;
#define COUNT_COMP(T, name) ++n;
      COMPONENT_LIST(COUNT_COMP)
#undef COUNT_COMP
      return n;
    }();

    ar(component_count);

#define SAVE_COMP(T, cname)                                                    \
  {                                                                            \
    std::ostringstream block_buf;                                              \
    {                                                                          \
      cereal::BinaryOutputArchive block_ar{block_buf};                        \
      auto view = registry.view<T>();                                          \
      uint32_t entity_count = static_cast<uint32_t>(view.size());             \
      block_ar(entity_count);                                                  \
      for (auto entity : view) {                                               \
        block_ar(entity);                                                      \
        FieldOutputArchive foa;                                                \
        detail::save_component<T>(registry, entity, foa);                     \
        foa.writeTo(block_ar);                                                 \
      }                                                                        \
    }                                                                          \
    std::string block_data = block_buf.str();                                  \
    std::string component_name{cname};                                         \
    uint64_t block_size = static_cast<uint64_t>(block_data.size());           \
    ar(component_name);                                                        \
    ar(block_size);                                                            \
    if (block_size > 0) {                                                      \
      ar(cereal::binary_data(block_data.data(), block_size));                  \
    }                                                                          \
  }

    COMPONENT_LIST(SAVE_COMP)
#undef SAVE_COMP
  }

  // -------------------------------------------------------------------------
  // V2 load: manifest-based, named-field format.
  // Unknown component names are skipped (removed from code).
  // Missing fields within a component keep their C++ default values.
  // -------------------------------------------------------------------------
  template <class Archive> void load_v2(Archive &ar) {
    uint32_t component_count = 0;
    ar(component_count);

    for (uint32_t i = 0; i < component_count; i++) {
      std::string component_name;
      uint64_t block_size = 0;
      ar(component_name);
      ar(block_size);

      if (block_size == 0) {
        continue;
      }

      std::string block_data(block_size, '\0');
      ar(cereal::binary_data(block_data.data(), block_size));

      std::istringstream block_stream(block_data);
      cereal::BinaryInputArchive block_ar{block_stream};

      uint32_t entity_count = 0;
      block_ar(entity_count);

      if (entity_count == 0) {
        continue;
      }

      bool dispatched = false;

#define LOAD_COMP(T, cname)                                                    \
  if (!dispatched && component_name == (cname)) {                              \
    dispatched = true;                                                         \
    for (uint32_t ei = 0; ei < entity_count; ei++) {                           \
      entt::entity entity{entt::null};                                         \
      block_ar(entity);                                                        \
      if (!registry.valid(entity)) {                                           \
        (void)registry.create(entity);                                         \
      }                                                                        \
      FieldInputArchive fia;                                                   \
      fia.readFrom(block_ar);                                                  \
      detail::emplace_component<T>(registry, entity, fia);                     \
    }                                                                          \
  }

      COMPONENT_LIST(LOAD_COMP)
#undef LOAD_COMP

      if (!dispatched) {
        log.warn("Skipping unknown component '{}' ({} bytes)", component_name,
                 block_size);
      }
    }
  }

  // -------------------------------------------------------------------------
  // V1 load: original hardcoded snapshot_loader chain (backward compat).
  // Files written by old code are transparently loaded; the next save()
  // will upgrade them to v2 format automatically.
  // -------------------------------------------------------------------------
  template <class Archive> void load_v1(Archive &ar) {
    // NOTE: v1 format is no longer loadable after component removal.
    // Kept as a stub so the code compiles; v2 is the active format.
    (void)ar;
    log.warn("v1 save format is no longer supported — skipping load");
  }

  // -------------------------------------------------------------------------
  // cereal entry points
  // -------------------------------------------------------------------------
  template <class Archive> void load(Archive &ar) {
    ar(cereal::base_class<Store>(this));
    if (file_version <= 1) {
      log.info("Loading v1 legacy format — will upgrade on next save");
      load_v1(ar);
    } else {
      load_v2(ar);
    }
    if (file_version == 3 && expected_type == 3) {
      std::map<std::string, int64_t> legacy_spendable;
      ar(legacy_spendable);
      apply_legacy_spendable_to_registry(std::move(legacy_spendable));
    }
  }

  template <class Archive> void save(Archive &ar) const {
    ar(cereal::base_class<Store>(this));
    save_v2(ar);
  }

  void apply_legacy_spendable_to_registry(std::map<std::string, int64_t> legacy) {
    entt::entity econ = entt::null;
    for (auto e : registry.view<SpendablePool>()) {
      econ = e;
      break;
    }
    if (econ == entt::null) {
      econ = registry.create();
      hf::meta m;
      m.name = "Economy";
      m.id = "ECONOMY";
      registry.emplace<hf::meta>(econ, m);
      registry.emplace<SpendablePool>(econ, SpendablePool{std::move(legacy)});
      return;
    }
    registry.get<SpendablePool>(econ).amounts = std::move(legacy);
  }

public:
  entt::registry registry;

  using Store::Store;
};
