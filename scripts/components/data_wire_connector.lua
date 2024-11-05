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
	api = {
		getConnectedFrames = function()
			frames = oracle:getWiredFrames(frame.data.id)
			return frames
		end,
	},
}
