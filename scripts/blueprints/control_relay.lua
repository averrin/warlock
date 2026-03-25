return {
	name = "Control Relay",
	size = FrameSize.S,
	components = {
		"Core",
		"Control Relay",
		"Data Wire Connector",
	},
	code = [[
local function find_component_by_id(fid)
  for _, c in ipairs(frame.components) do
    if c.data.id == fid then return c end
  end
  return nil
end

return {
  start = function()
    relay = frame:getComponentByType("Control Relay")
    tick_counter = 0
  end,
  update = function()
    if relay == nil or relay.state ~= ComponentState.ACTIVE then return end

    local interval_attr = relay.data.attributes["heartbeat_interval"]
    local interval = interval_attr and interval_attr:GetFinalValue() or 60

    tick_counter = tick_counter + 1
    if tick_counter < interval then return end
    tick_counter = 0

    -- Find the targeted data connector
    local target_attr = relay.data.attributes["target"]
    local target_id = target_attr and target_attr:GetFinalValue() or -1
    local dc = nil
    if target_id >= 0 then
      dc = find_component_by_id(target_id)
    end
    -- Fallback: use first available data connector
    if dc == nil then
      dc = frame:getComponentByType("Data Connector")
    end
    if dc == nil then return end

    dc:send({
      source = frame.data.id,
      destination = -1,
      headers = { type = "CONTROL_HEARTBEAT" },
      body = tostring(frame.data.id),
    })
  end,
  idle = function() end,
  stop = function() end,
}
]],
}
