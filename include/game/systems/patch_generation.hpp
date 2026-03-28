#pragma once

#include <random>
#include <utility>
#include <vector>

/** Generate a blob shape using cellular automata with the given seeded RNG. */
std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds,
  std::mt19937& rng
);

/** Convenience overload — creates its own RNG from std::random_device. */
std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
);
