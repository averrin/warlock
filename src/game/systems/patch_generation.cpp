#include <game/systems/patch_generation.hpp>
#include <random>
#include <algorithm>

std::vector<std::pair<int, int>> generateBlob(
  int width, int height,
  float fill_probability,
  int smoothing_rounds
) {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0f, 1.0f);
  
  std::vector<std::vector<bool>> grid(height, std::vector<bool>(width, false));
  
  // Initial random fill
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      grid[y][x] = dis(gen) < fill_probability;
    }
  }
  
  // Cellular automata smoothing
  for (int round = 0; round < smoothing_rounds; ++round) {
    std::vector<std::vector<bool>> next = grid;
    
    for (int y = 0; y < height; ++y) {
      for (int x = 0; x < width; ++x) {
        int neighbors = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
              if (grid[ny][nx]) neighbors++;
            }
          }
        }
        
        if (grid[y][x]) {
          next[y][x] = neighbors >= 4;
        } else {
          next[y][x] = neighbors >= 6;
        }
      }
    }
    
    grid = next;
  }
  
  // Convert to coordinate list
  std::vector<std::pair<int, int>> result;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (grid[y][x]) {
        result.emplace_back(x, y);
      }
    }
  }
  
  // Fallback: if smoothing eliminated all cells, create a small core
  if (result.empty()) {
    int cx = width / 2;
    int cy = height / 2;
    result.emplace_back(cx, cy);
    if (cx > 0) result.emplace_back(cx - 1, cy);
    if (cx < width - 1) result.emplace_back(cx + 1, cy);
    if (cy > 0) result.emplace_back(cx, cy - 1);
    if (cy < height - 1) result.emplace_back(cx, cy + 1);
  }
  
  return result;
}
