#pragma once

#include <game/attributes.hpp>
#include <optional>
#include <string>
#include <vector>

class GameManager;

namespace game {

/** Replaces `attr`'s modifier list with instances resolved from names. Returns an error message on failure. */
std::optional<std::string> ApplyAttributeModifierNames(
    Attribute &attr,
    const std::vector<std::string> &names,
    GameManager &gm);

} // namespace game
