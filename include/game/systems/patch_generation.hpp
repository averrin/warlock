#pragma once

#include <utility>
#include <vector>

std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
);
