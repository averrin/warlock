#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <variant>
#include <vector>

// Define a variant type for the attribute that can be int, float, or string
using AttributeValue = std::variant<int, float, std::string, bool>;

// Enum to represent the type of the attribute
enum class AttributeType { INT, FLOAT, STRING, BOOL };

// Forward declaration of the Attribute class
class Attribute;

// Modifier class representing different types of modifications
class Modifier {
public:
  // A function that modifies the attribute
  using ModifierFunction = std::function<AttributeValue(const Attribute &)>;

private:
  ModifierFunction modFunction;

public:
  // Constructor accepting a lambda function that manipulates the attribute
  Modifier(ModifierFunction func) : modFunction(func) {}

  // Applies the lambda function to the attribute and returns the modified value
  AttributeValue Apply(const Attribute &attribute) const {
    return modFunction(attribute);
  }
};

// Attribute class to store a variant value (int, float, or string) and compute
// final value
class Attribute {
private:
  std::string title;
  std::string description;
  AttributeType type;
  AttributeValue baseValue;
  std::vector<std::shared_ptr<Modifier>> modifiers;

public:
  // Constructor that accepts title, description, type, and base value
  Attribute(const std::string &t, const std::string &d, AttributeType at,
            AttributeValue base)
      : title(t), description(d), type(at), baseValue(base) {}

  // Adds a modifier to the list
  void AddModifier(std::shared_ptr<Modifier> modifier) {
    modifiers.push_back(modifier);
  }

  // Removes all modifiers
  void ClearModifiers() { modifiers.clear(); }

  // Computes the final value after applying all modifiers
  AttributeValue GetFinalValue() const {
    AttributeValue finalValue = baseValue;
    for (const auto &modifier : modifiers) {
      finalValue = modifier->Apply(*this);
    }
    return finalValue;
  }

  // Getters for title, description, and type
  const std::string &GetTitle() const { return title; }

  const std::string &GetDescription() const { return description; }

  AttributeType GetType() const { return type; }

  // Sets the base value
  void SetBaseValue(AttributeValue base) { baseValue = base; }

  // Gets the base value
  const AttributeValue &GetBaseValue() const { return baseValue; }

  std::string ToString() const {
    std::string result = title + ": ";
    switch (type) {
    case AttributeType::INT:
      result += std::to_string(std::get<int>(baseValue));
      break;
    case AttributeType::FLOAT:
      result += std::to_string(std::get<float>(baseValue));
      break;
    case AttributeType::STRING:
      result += std::get<std::string>(baseValue);
      break;
    case AttributeType::BOOL:
      result += std::get<bool>(baseValue) ? "true" : "false";
      break;
    }
    return result;
  }
};
