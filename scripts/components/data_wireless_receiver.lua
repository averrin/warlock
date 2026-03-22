return {
	name = "Data Wireless Receiver",
	category = "Connectors",
	description = "Receives data wirelessly from an emitter.",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Data Wireless Receiver",
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
		readRaw = function(component)
			return component:readRaw()
		end,
		read = function(component)
			return component:read()
		end,
	},
}
