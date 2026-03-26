return {
	name = "Data Wireless Emitter",
	category = "Connectors",
	description = "Emits data wirelessly to receivers within radius.",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Data Wireless Emitter",
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
	spendable_cost = { ["Electronic Parts"] = 10 },
	require = { "Core" },
	api = {
		sendRaw = function(component, s)
			component:sendRaw(s)
		end,
		send = function(component, s)
			component:send(s)
		end,
	},
}
