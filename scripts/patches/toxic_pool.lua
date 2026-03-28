return {
  z_index = 3,
  name = "Toxic Pool",
  description = "Pool of corrosive alien chemicals — blocks frames and connections",
  item = "",
  obstacle = true,
  color = { r = 50, g = 200, b = 60, a = 160 },
  generation = {
    min_width = 8,
    max_width = 18,
    min_height = 8,
    max_height = 18,
    fill_probability = 0.75,
    smoothing_rounds = 4,
  }
}
