return {
	name = "Control Relay",
	category = "Infrastructure",
	description = "Extends the control zone when reachable from Nexus via data network.",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Control Relay",
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
		control_radius = {
			title = "Control Radius",
			description = "Placement and visibility radius in grid cells",
			type = AttributeType.INT,
			value = 20,
		},
		heartbeat_interval = {
			title = "Heartbeat Interval",
			description = "Ticks between heartbeat packets",
			type = AttributeType.INT,
			value = 60,
		},
		target = {
			title = "Target data connector",
			description = "Component id of data connector used for sending heartbeat packets",
			type = AttributeType.INT,
			value = -1,
			target_filter = "Data Connector",
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.S,
	require = { "Core" },
	api = {},
}
