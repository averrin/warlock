return {
	name = "Power Wireless Emitter",
	category = "Connectors",
	description = "Emits power wirelessly to receivers within radius.",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Power Wireless Emitter",
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
		radius = {
			title = "Radius",
			description = "Max connection radius in grid cells",
			type = AttributeType.FLOAT,
			value = 10.0,
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
			return attr(component, "connections")
		end,
	},
}
