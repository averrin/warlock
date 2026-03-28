return {
  z_index = 3,
  name = "Impact Crater",
  description = "Circular depression from ancient meteorite impact — impassable terrain",
  item = "",
  obstacle = true,
  color = { r = 60, g = 55, b = 50, a = 180 },
  generation = {
    min_width = 10,
    max_width = 22,
    min_height = 10,
    max_height = 22,
    fill_probability = 0.85,
    smoothing_rounds = 5,
  }
}
