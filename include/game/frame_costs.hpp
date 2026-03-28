#pragma once

#include <game/components/frame.hpp>
#include <map>
#include <string>

namespace warlock {

inline std::map<std::string, int> frame_cost_for_size(FrameSize sz) {
  switch (sz) {
  case FrameSize::XS:
    return {{"Ultralight Structures", 10}};
  case FrameSize::S:
    return {{"Frame Parts", 25}};
  case FrameSize::M:
    return {{"Frame Parts", 50}};
  case FrameSize::L:
    return {{"Frame Parts", 100}};
  case FrameSize::G:
    return {{"Frame Parts", 50}, {"Ultralight Structures", 50}};
  default:
    return {};
  }
}

} // namespace warlock
