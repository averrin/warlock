return {
  name = "Crater Cluster",
  stage = 3,
  place = function(ctx)
    local clusters = ctx.rng_int(1, 2)
    for c = 1, clusters do
      local cx = ctx.rng_int(math.floor(ctx.width * 0.3), math.floor(ctx.width * 0.7))
      local cy = ctx.rng_int(math.floor(ctx.height * 0.3), math.floor(ctx.height * 0.7))
      local count = ctx.rng_int(3, 5)
      for i = 1, count do
        local ox = cx + ctx.rng_int(-15, 15)
        local oy = cy + ctx.rng_int(-15, 15)
        if not ctx.is_occupied(ox, oy) then
          ctx.place_patch("crater", ox, oy)
        end
      end
    end
  end,
}
