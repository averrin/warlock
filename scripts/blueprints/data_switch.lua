return {
	name = "Data Switch",
	size = FrameSize.M,
	components = {
		"Core",
		"Data Wire Connector",
		"Data Wire Connector",
		"Data Wire Connector",
		"Data Wire Connector",
	},
	code = [[
local function is_tap(c)
  local a = c.data.attributes["tap"]
  return a and a:GetFinalValue()
end
return {
  start = function()
    ports = {}
    taps = {}
    local all = frame:getComponentsByType("Data Connector")
    for _, c in ipairs(all) do
      if is_tap(c) then
        table.insert(taps, c)
      else
        table.insert(ports, c)
      end
    end
    table.sort(ports, function(a, b) return a.data.id < b.data.id end)
    table.sort(taps, function(a, b) return a.data.id < b.data.id end)
    counters = { rx = {}, tx = {} }
    max_queue_seen = {}
    for i = 1, #ports do
      counters.rx[i] = 0
      counters.tx[i] = 0
      max_queue_seen[i] = 0
    end
    DRAIN_CAP = 64
  end,
  update = function()
    for i, ingress in ipairs(ports) do
      local d = ingress:queueDepthRaw() + ingress:queueDepthPacket()
      if d > max_queue_seen[i] then max_queue_seen[i] = d end
      local n = 0
      while n < DRAIN_CAP do
        local raw = ingress:readRaw()
        if raw ~= nil then
          counters.rx[i] = counters.rx[i] + 1
          for j, p in ipairs(ports) do
            if j ~= i then
              p:injectRaw(raw)
              counters.tx[j] = counters.tx[j] + 1
            end
          end
          for _, t in ipairs(taps) do
            t:injectRaw(raw)
          end
          n = n + 1
        else
          local pkt = ingress:read()
          if pkt == nil then break end
          counters.rx[i] = counters.rx[i] + 1
          for j, p in ipairs(ports) do
            if j ~= i then
              p:injectPacket(pkt)
              counters.tx[j] = counters.tx[j] + 1
            end
          end
          for _, t in ipairs(taps) do
            t:injectPacket(pkt)
          end
          n = n + 1
        end
      end
    end
  end,
  idle = function() end,
  stop = function() end,
}
]],
}
