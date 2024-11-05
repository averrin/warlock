return {
	name = "Packer",
	description = "Standard packer",
	attributes = {
		type = {
			title = "Type",
			value = "Packer",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 200.0,
		},
		recipe = {
			title = "Recipe",
			description = "Active recipe",
			type = AttributeType.STRING,
			value = "Compacted Carbon Dust",
		},
		heat = {
			title = "Heat",
			description = "Heat produced",
			type = AttributeType.FLOAT,
			value = 0.3,
			easing = {
				type = AttributeEasingType.SIN,
				range = 0.3,
				period = 650.0,
			},
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	api = {},
}
