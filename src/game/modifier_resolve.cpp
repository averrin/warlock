#include <game/modifier_resolve.hpp>
#include <effolkronium/random.hpp>
#include <game/game_manager.hpp>
#include <game/systems/items.hpp>
#include <unordered_set>

using Random = effolkronium::random_static;

namespace game {

std::optional<std::string> ApplyAttributeModifierNames(
    Attribute &attr,
    const std::vector<std::string> &names,
    GameManager &gm) {
  std::unordered_set<std::string> seen;
  for (const auto &n : names) {
    if (!seen.insert(n).second) {
      return "Duplicate modifier: " + n;
    }
  }

  std::vector<std::shared_ptr<Modifier>> resolved;
  resolved.reserve(names.size());

  for (const auto &name : names) {
    if (name == "Overheat") {
      if (attr.GetType() != AttributeType::FLOAT) {
        return "Overheat requires FLOAT attribute";
      }
      resolved.push_back(std::make_shared<Modifier>("Overheat", [](const Attribute &a) {
        return std::get<float>(a.GetBaseValue()) * 0.5f;
      }));
    } else if (name == "Freeze") {
      if (attr.GetType() != AttributeType::FLOAT) {
        return "Freeze requires FLOAT attribute";
      }
      resolved.push_back(std::make_shared<Modifier>("Freeze", [](const Attribute &a) {
        return std::get<float>(a.GetBaseValue()) * 1.5f;
      }));
    } else if (name == "Jitter") {
      if (attr.GetType() != AttributeType::FLOAT) {
        return "Jitter requires FLOAT attribute";
      }
      resolved.push_back(std::make_shared<Modifier>("Jitter", [](const Attribute &a) {
        auto b = std::get<float>(a.GetBaseValue());
        auto mod = 1.f - a.easing.easing_range;
        return Random::get<float>(mod * b, b);
      }));
    } else {
      if (attr.GetType() != AttributeType::FLOAT) {
        return "Recipe modifiers require FLOAT attribute";
      }
      if (!gm.items) {
        return "Items system not available";
      }
      auto mod = gm.items->sharedRecipeModifier(name);
      if (!mod) {
        return "Unknown modifier or recipe has no power cost: " + name;
      }
      resolved.push_back(mod);
    }
  }

  attr.ClearModifiers();
  for (const auto &m : resolved) {
    attr.AddModifier(m);
  }
  return std::nullopt;
}

} // namespace game
