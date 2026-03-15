#pragma once
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tweeny.h>
#include <variant>
#include <vector>
using tweeny::easing;
#include <effolkronium/random.hpp>
using Random = effolkronium::random_static;
#include <fmt/core.h>
#include <fmt/format.h>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

// Define a variant type for the attribute that can be int, float, or string
using AttributeValue = std::variant<int, float, std::string, bool>;

// Enum to represent the type of the attribute
enum class AttributeType { INT, FLOAT, STRING, BOOL };
enum class AttributeEasingType { NONE, JITTER, SAW, SIN, RANDOM_STEP };

// namespace cereal {
//     template<class Archive>
//     void save(Archive & ar, const AttributeType & t) {
//         ar(static_cast<std::underlying_type_t<AttributeType>>(t));
//     }

//     template<class Archive>
//     void load(Archive & ar, AttributeType & t) {
//         std::underlying_type_t<AttributeType> underlying;
//         ar(underlying);
//         t = static_cast<AttributeType>(underlying);
//     }
// }

// // Serialization for AttributeEasingType enum
// namespace cereal {
//     template<class Archive>
//     void save(Archive & ar, const AttributeEasingType & t) {
//         ar(static_cast<std::underlying_type_t<AttributeEasingType>>(t));
//     }

//     template<class Archive>
//     void load(Archive & ar, AttributeEasingType & t) {
//         std::underlying_type_t<AttributeEasingType> underlying;
//         ar(underlying);
//         t = static_cast<AttributeEasingType>(underlying);
//     }
// }

// Forward declaration of the Attribute class
class Attribute;

// Modifier class representing different types of modifications
class Modifier {
public:
  // A function that modifies the attribute
  using ModifierFunction = std::function<AttributeValue(const Attribute &)>;
  std::string name = "";

private:
  ModifierFunction modFunction;

public:
  // Constructor accepting a lambda function that manipulates the attribute
  Modifier(std::string name, ModifierFunction func)
      : name(name), modFunction(func) {}

  // Applies the lambda function to the attribute and returns the modified value
  AttributeValue Apply(const Attribute &attribute) const {
    return modFunction(attribute);
  }
};

struct AttributeEasing {
  AttributeEasingType easing_type = AttributeEasingType::NONE;
  float easing_range = 0.2f;
  float easing_period = 1000.f;
  tweeny::tween<float> tween;
};

// Attribute class to store a variant value (int, float, or string) and compute
// final value
class Attribute {
private:
  std::string title;
  std::string description;
  AttributeType type;
  AttributeValue baseValue;

public:
  std::vector<std::shared_ptr<Modifier>> modifiers = {};
  AttributeEasing easing;
  // Constructor that accepts title, description, type, and base value
  Attribute(const std::string &t, const std::string &d, AttributeType at,
            AttributeValue base, AttributeEasing easing = AttributeEasing())
      : title(t), description(d), type(at), baseValue(base) {
    SetEasing(easing);
  }

  Attribute() {}
  Attribute(const Attribute &other)
      : title(other.title), description(other.description), type(other.type),
        baseValue(other.baseValue) {
    SetEasing(other.easing);
  }

  // Adds a modifier to the list
  void AddModifier(std::shared_ptr<Modifier> modifier) {
    if (std::find(modifiers.begin(), modifiers.end(), modifier) !=
        modifiers.end()) {
      return;
    }
    modifiers.push_back(modifier);
  }
  void RemoveModifier(std::shared_ptr<Modifier> modifier) {
    if (std::find(modifiers.begin(), modifiers.end(), modifier) !=
        modifiers.end()) {
      modifiers.erase(std::remove(modifiers.begin(), modifiers.end(), modifier),
                      modifiers.end());
    }
  }

  void SetEasing(AttributeEasing e) {
    easing = e;
    if (easing.easing_type == AttributeEasingType::NONE)
      return;
    auto d = easing.easing_period;
    auto bot = 1.f - easing.easing_range;
    auto p = std::get<float>(baseValue);
    if (easing.easing_type == AttributeEasingType::SAW) {
      easing.tween = tweeny::from(p * bot)
                         .to(p * 1.f)
                         .during(d / 2.f)
                         .via(easing::linear)
                         .to(p * bot)
                         .during(d / 2.f)
                         .via(easing::linear);
    } else if (easing.easing_type == AttributeEasingType::SIN) {
      easing.tween = tweeny::from(p * bot)
                         .to(p * 1.f)
                         .during(d / 2.f)
                         .via(easing::sinusoidalInOut)
                         .to(p * bot)
                         .during(d / 2.f)
                         .via(easing::sinusoidalInOut);
    } else if (easing.easing_type == AttributeEasingType::JITTER) {
      AddModifier(
          std::make_shared<Modifier>("Jitter", [](const Attribute &attr) {
            auto b = std::get<float>(attr.GetBaseValue());
            auto mod = 1.f - attr.easing.easing_range;
            return Random::get<float>(mod * b, b);
          }));
    }
  }

  void resetEasing() {
    if (easing.easing_type == AttributeEasingType::NONE) {
      return;
    }
    if (easing.easing_type == AttributeEasingType::JITTER) {
      return;
    }
    easing.tween.seek(0);
  }

  void update(std::chrono::duration<double, std::milli> delta) {
    if (easing.easing_type == AttributeEasingType::NONE) {
      return;
    }
    if (easing.easing_type == AttributeEasingType::JITTER) {
      return;
    }
    auto step = static_cast<int32_t>(delta.count());
    easing.tween.step(step);
    if (easing.tween.progress() >= 1.f) {
      easing.tween.seek(0);
    }
  }

  // Removes all modifiers
  void ClearModifiers() { modifiers.clear(); }

  // Computes the final value after applying all modifiers
  AttributeValue GetFinalValue() const {
    AttributeValue finalValue = baseValue;

    if (easing.easing_type != AttributeEasingType::NONE) {
      finalValue = easing.tween.peek();
    }
    for (const auto &modifier : modifiers) {
      finalValue = modifier->Apply(*this);
    }
    return finalValue;
  }

  // Getters for title, description, and type
  const std::string &GetTitle() const { return title; }

  const std::string &GetDescription() const { return description; }

  AttributeType GetType() const { return type; }

  template <class T> T get() const { return std::get<T>(GetFinalValue()); }
  template <class T> T getBase() const { return std::get<T>(GetBaseValue()); }

  // Sets the base value
  void SetBaseValue(AttributeValue base) {
    baseValue = base;
    SetEasing(easing);
  }

  // Gets the base value
  const AttributeValue &GetBaseValue() const { return baseValue; }

  std::string ToString() const {
    std::string result = title + ": ";
    switch (type) {
    case AttributeType::INT:
      result += std::to_string(get<int>());
      break;
    case AttributeType::FLOAT:
      result += std::to_string(get<float>());
      break;
    case AttributeType::STRING:
      result += get<std::string>();
      break;
    case AttributeType::BOOL:
      result += get<bool>() ? "true" : "false";
      break;
    }
    return result;
  }
  friend class cereal::access;

  template <class Archive> void save(Archive &ar) const {
    auto t = static_cast<int>(type);
    auto et = static_cast<int>(easing.easing_type);
    ar(title, description, type, baseValue, easing.easing_type,
       easing.easing_range, easing.easing_period);
  }

  template <class Archive> void load(Archive &ar) {
    ar(title, description, type, baseValue, easing.easing_type,
       easing.easing_range, easing.easing_period);
    SetEasing(easing); // Reconstruct the easing tween after loading
  }
};
