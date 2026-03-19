return {
	name = "Power Wire Connector",
	category = "Connectors",
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
		max_connections = {
			title = "Max Connections",
			description = "Maximum number of connections",
			type = AttributeType.INT,
			value = 10,
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
	require = {},
	api = {
		getConnections = function(component)
			return component.data.attributes["connections"]:GetFinalValue()
		end,
	},
}
