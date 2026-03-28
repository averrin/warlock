return {
  name = "Ridge Line",
  stage = 3,
  place = function(ctx)
    -- Create a ridge of large rocks from one edge to another
    local ridges = ctx.rng_int(0, 1)
    for r = 1, ridges do
      -- Pick start and end on opposite edges
      local x1 = ctx.rng_int(0, ctx.width - 1)
      local y1 = 0
      local x2 = ctx.rng_int(0, ctx.width - 1)
      local y2 = ctx.height - 1

      -- Horizontal or vertical ridge
      if ctx.rng_int(0, 1) == 1 then
        x1 = 0
        y1 = ctx.rng_int(0, ctx.height - 1)
        x2 = ctx.width - 1
        y2 = ctx.rng_int(0, ctx.height - 1)
      end

      -- Walk from start to end with random perturbation
      local steps = ctx.rng_int(4, 8)
      for s = 0, steps do
        local t = s / steps
        local px = math.floor(x1 + (x2 - x1) * t + ctx.rng_int(-8, 8))
        local py = math.floor(y1 + (y2 - y1) * t + ctx.rng_int(-8, 8))
        px = math.max(0, math.min(ctx.width - 1, px))
        py = math.max(0, math.min(ctx.height - 1, py))
        if not ctx.is_occupied(px, py) then
          ctx.place_patch("rocks_big", px, py)
        end
      end
    end
  end,
}
