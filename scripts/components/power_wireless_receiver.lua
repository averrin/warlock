return {
	name = "Power Wireless Receiver",
	category = "Connectors",
	description = "Receives power wirelessly from an emitter.",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Power Wireless Receiver",
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
			value = 1,
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
