return {
  name = "Dried Riverbed",
  stage = 5,
  place = function(ctx)
    -- Winding path of carbon seam patches across the map
    local rivers = ctx.rng_int(0, 1)
    for r = 1, rivers do
      -- Random walk from one side to the other
      local x = ctx.rng_int(0, 10)
      local y = ctx.rng_int(math.floor(ctx.height * 0.2), math.floor(ctx.height * 0.8))
      local target_x = ctx.width - ctx.rng_int(0, 10)

      while x < target_x do
        if not ctx.is_occupied(x, y) then
          ctx.place_patch("carbon_seam", x, y)
        end
        -- Move mostly right with some vertical drift
        x = x + ctx.rng_int(15, 30)
        y = y + ctx.rng_int(-12, 12)
        y = math.max(5, math.min(ctx.height - 5, y))
      end
    end
  end,
}
