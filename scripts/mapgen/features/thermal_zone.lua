return {
  name = "Thermal Zone",
  stage = 4,
  place = function(ctx)
    local zones = ctx.rng_int(1, 2)
    for z = 1, zones do
      local cx = ctx.rng_int(0, ctx.width - 1)
      local cy = ctx.rng_int(0, ctx.height - 1)
      local count = ctx.rng_int(3, 8)
      for i = 1, count do
        local ox = cx + ctx.rng_int(-10, 10)
        local oy = cy + ctx.rng_int(-10, 10)
        if not ctx.is_occupied(ox, oy) then
          ctx.place_patch("geothermal_vent", ox, oy)
        end
      end
    end
  end,
}
