return {
  width = 200,
  height = 200,
  seed = 0,                        -- 0 = random seed
  starter_radius = 10,             -- clear zone around spawn (cells)
  biome_seed_count = 5,            -- Voronoi region count
  max_overlap_fraction = 0.3,      -- max cell overlap before retry
  max_placement_attempts = 50,     -- retries per patch placement
  min_same_type_distance = 20,     -- min cells between same-type patches

  -- Guaranteed starter resources near spawn
  starter = {
    { type = "sparkstone_deposit", distance = 12 },
    { type = "copper_deposit",     distance = 14 },
    { type = "carbon_seam",        distance = 16 },
  },

  -- Base resource counts per map (scaled by biome weights)
  base_resources = {
    sparkstone_deposit  = { count = {4, 8} },
    copper_deposit      = { count = {3, 6} },
    iron_vein           = { count = {3, 5} },
    carbon_seam         = { count = {2, 4} },
    crystal_field       = { count = {2, 4} },
    rare_earth_deposit  = { count = {1, 3} },
  },

  -- Base obstacle counts
  base_obstacles = {
    rocks_small   = { count = {6, 12} },
    rocks_big     = { count = {3, 6} },
    lake          = { count = {1, 3} },
    crater        = { count = {2, 5} },
    toxic_pool    = { count = {1, 3} },
    lava_flow     = { count = {1, 3} },
    alien_ruins   = { count = {1, 2} },
  },
}
