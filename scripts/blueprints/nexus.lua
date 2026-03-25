return {
	name = "Nexus",
	size = FrameSize.M,
	components = {
		"Main Core",
		"Nexus",
		"Power Wire Connector",
		"Life Support",
		"Data Wire Connector",
	},
	code = [[
local HEARTBEAT_TIMEOUT = 180  -- ticks before a relay is considered offline
local DRAIN_CAP = 64           -- max packets to drain per update

local function find_component_by_id(fid)
  for _, c in ipairs(frame.components) do
    if c.data.id == fid then return c end
  end
  return nil
end

return {
  start = function()
    nexus = frame:getComponentByType("Nexus")
    alive_relays = {}   -- frame_id -> last_seen_tick
    tick = 0
  end,
  update = function()
    tick = tick + 1
    if nexus == nil or nexus.state ~= ComponentState.ACTIVE then return end

    -- Find targeted data connector for heartbeat reception
    local target_attr = nexus.data.attributes["target_data"]
    local target_id = target_attr and target_attr:GetFinalValue() or -1
    local dc = nil
    if target_id >= 0 then
      dc = find_component_by_id(target_id)
    end
    if dc == nil then
      dc = frame:getComponentByType("Data Connector")
    end
    if dc == nil then return end

    -- Drain packets, looking for heartbeats
    local n = 0
    while n < DRAIN_CAP do
      local pkt = dc:read()
      if pkt == nil then break end
      n = n + 1
      if pkt.headers and pkt.headers["type"] == "CONTROL_HEARTBEAT" then
        local relay_frame_id = tonumber(pkt.body)
        if relay_frame_id then
          alive_relays[relay_frame_id] = tick
        end
      end
    end

    -- Expire stale relays
    for fid, last_seen in pairs(alive_relays) do
      if tick - last_seen > HEARTBEAT_TIMEOUT then
        alive_relays[fid] = nil
      end
    end

    -- Write alive relay frame IDs into the nexus attribute for C++ consumption
    local ids = {}
    for fid, _ in pairs(alive_relays) do
      table.insert(ids, tostring(fid))
    end
    local relays_attr = nexus.data.attributes["alive_relays"]
    if relays_attr then
      relays_attr:SetBaseValue(table.concat(ids, ","))
    end
  end,
  idle = function() end,
  stop = function() end,
}
]],
}
