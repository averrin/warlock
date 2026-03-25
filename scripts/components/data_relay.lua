return {
	name = "Data Relay",
	category = "Connectors",
	description = "Bridges two data links on one XS frame (transparent hop between neighbors).",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Data Connector",
		},
		max_connections = {
			title = "Max Connections",
			description = "Maximum number of connections",
			type = AttributeType.INT,
			value = 2,
		},
		max_connection_distance = {
			title = "Connection radius",
			description = "Max center-to-center distance to connect (world units).",
			type = AttributeType.FLOAT,
			value = 500.0,
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
	require = {},
	api = {},
}
