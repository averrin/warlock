return {
	name = "Conveyor Relay",
	category = "Connectors",
	description = "Two conveyor links on one XS frame; bridges neighbors (throughput = min of both links).",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Conveyor Connector",
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
