return {
	name = "Cooler",
	category = "Thermal",
	description = "Generic cooler",
	icon = "thermometer-cold.png",
	attributes = {
		type = {
			title = "Type",
			value = "Temp Control",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 250.0,
		},
		heat = {
			title = "Active heat",
			description = "Heat generated while active",
			type = AttributeType.FLOAT,
			value = -0.75,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	api = {},
}
