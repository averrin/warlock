return {
	name = "Data Wire Connector",
	category = "Connectors",
	description = "Provide data link (and power) for adjusting frames.",
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
			value = 1,
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
		tap = {
			title = "Tap (mirror only)",
			description = "If true, switch cores treat this as a mirror port: no ingress flooding, only copies from other ports.",
			type = AttributeType.BOOL,
			value = false,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	require = { "Core" },
	api = {
		getConnectedFrames = function()
			return {}
		end,
	},
}
