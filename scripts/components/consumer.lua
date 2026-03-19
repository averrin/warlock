return {
	name = "Consumer",
	category = "Power",
	description = "Generic consumer",
	attributes = {
		type = {
			title = "Type",
			value = "Consumer",
		},
		load = {
			title = "Load",
			description = "Component load",
			type = AttributeType.FLOAT,
			value = 1.0,
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 940.0,
			easing = {
				type = AttributeEasingType.SIN,
				range = 0.1,
				period = 650.0,
			},
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	require = { "Power Wire Connector" },
	api = {},
}
