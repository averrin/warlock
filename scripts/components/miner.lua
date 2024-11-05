return {
	name = "Miner",
	description = "Standard miner",
	attributes = {
		type = {
			title = "Type",
			value = "Miner",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 750.0,
		},
		recipe = {
			title = "Recipe",
			description = "Active recipe",
			type = AttributeType.STRING,
			value = "Spark Ore",
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
