#pragma once
#include <game/components/frame.hpp>
#include <utils/entt.hpp>
#include <utils/entt_lua.hpp>

class Oracle {
public:
  std::vector<Frame> getWiredFrames(int id);
};
