return {
	name = "Refinery",
	description = "Standard refinery",
	attributes = {
		type = {
			title = "Type",
			value = "Item Producer",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 500.0,
		},
		recipe = {
			title = "Recipe",
			description = "Active recipe",
			type = AttributeType.STRING,
			value = "Spark Stone",
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
