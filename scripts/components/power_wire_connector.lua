return {
	name = "Power Wire Connector",
	description = "Power Wire Connector",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Power Wire Connector",
		},
		stable = {
			title = "Stable",
			description = "Does continue work after power loss",
			type = AttributeType.BOOL,
			value = true,
		},
		connections = {
			title = "Connections",
			description = "Number of connections",
			type = AttributeType.INT,
			value = 0,
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
		getConnections = function(component)
			return component.data.attributes["connections"]:GetFinalValue()
		end,
	},
}
