return {
	name = "Data Wire Connector",
	description = "Provide data link (and power) for adjusting frames.",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Data Connector",
		},
		max_connections = {
			title = "Max Connections",
			description = "Maximum number of connections",
			type = AttributeType.INT,
			value = 1,
		},
		stable = {
			title = "Stable",
			description = "Does continue work after power loss",
			type = AttributeType.BOOL,
			value = true,
		},
		temp_proof = {
			title = "Temp Proof",
			description = "Does not overheat or freeze",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	require = { "Core" },
	api = {
		getConnectedFrames = function()
			frames = oracle:getWiredFrames(frame.data.id)
			return frames
		end,
	},
}
