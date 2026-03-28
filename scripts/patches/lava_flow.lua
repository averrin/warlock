return {
  z_index = 3,
  name = "Lava Flow",
  description = "Solidified lava channel — impassable terrain obstacle",
  item = "",
  obstacle = true,
  color = { r = 200, g = 60, b = 20, a = 170 },
  generation = {
    min_width = 25,
    max_width = 45,
    min_height = 2,
    max_height = 4,
    fill_probability = 0.70,
    smoothing_rounds = 2,
  }
}
