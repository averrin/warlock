return {
	name = "External AO Cooler",
	category = "Thermal",
	description = "Area cooler — affects nearby frames within radius (world grid)",
	icon = "thermometer-cold.png",
	attributes = {
		type = {
			title = "Type",
			value = "External AO Cooler",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 120.0,
		},
		heat = {
			title = "Thermal strength",
			description = "Signed influence (cooling is negative), scaled by distance falloff",
			type = AttributeType.FLOAT,
			value = -0.4,
		},
		radius = {
			title = "Radius",
			description = "World grid cells (25px)",
			type = AttributeType.INT,
			value = 8,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	api = {},
}
