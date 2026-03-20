#pragma once

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/version.hpp>
#include <utils/data/field_archive.hpp>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

struct ResourcePatch {
  std::string patch_type;
  std::string item_name;
  /** When true, blocks frame placement and WIRE/BEAM paths (terrain obstacle). */
  bool obstacle = false;
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
  template <class Archive> void serialize(Archive& ar, const std::uint32_t version) {
    ar(patch_type, item_name, cells, min_x, min_y, max_x, max_y);
    if (version >= 1) {
      ar(obstacle);
    } else {
      obstacle = false;
    }
  }

  void field_save(FieldOutputArchive& ar) const {
    FIELD(ar, patch_type);
    FIELD(ar, item_name);
    FIELD(ar, obstacle);
    FIELD(ar, cells);
    FIELD(ar, min_x);
    FIELD(ar, min_y);
    FIELD(ar, max_x);
    FIELD(ar, max_y);
  }
  void field_load(FieldInputArchive& ar) {
    FIELD(ar, patch_type);
    FIELD(ar, item_name);
    FIELD(ar, obstacle);
    FIELD(ar, cells);
    FIELD(ar, min_x);
    FIELD(ar, min_y);
    FIELD(ar, max_x);
    FIELD(ar, max_y);
  }
};

CEREAL_CLASS_VERSION(ResourcePatch, 1);
