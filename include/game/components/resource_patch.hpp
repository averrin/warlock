#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

struct ResourcePatch {
  std::string patch_type;
  std::string item_name;
  std::vector<std::pair<int, int>> cells;
  
  int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
  
  void recalculateBounds() {
    if (cells.empty()) {
      min_x = min_y = max_x = max_y = 0;
      return;
    }
    min_x = max_x = cells[0].first;
    min_y = max_y = cells[0].second;
    for (const auto& [x, y] : cells) {
      if (x < min_x) min_x = x;
      if (x > max_x) max_x = x;
      if (y < min_y) min_y = y;
      if (y > max_y) max_y = y;
    }
  }
  
  bool containsCell(int x, int y) const {
    return std::find(cells.begin(), cells.end(), std::make_pair(x, y)) != cells.end();
  }
  
  bool overlapsRect(int rx, int ry, int rw, int rh) const {
    for (const auto& [cx, cy] : cells) {
      if (cx >= rx && cx < rx + rw && cy >= ry && cy < ry + rh) {
        return true;
      }
    }
    return false;
  }
  
  friend class cereal::access;
  template <class Archive> void save(Archive &ar) const {
    ar(patch_type, item_name, cells, min_x, min_y, max_x, max_y);
  }
  template <class Archive> void load(Archive &ar) {
    ar(patch_type, item_name, cells, min_x, min_y, max_x, max_y);
  }
};
