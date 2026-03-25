return {
	name = "External AO Heater",
	category = "Thermal",
	description = "Area heater — affects nearby frames within radius (world grid)",
	icon = "thermometer-hot.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			value = "External AO Heater",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 120.0,
		},
		heat = {
			title = "Thermal strength",
			description = "Signed influence (heating is positive), scaled by distance falloff",
			type = AttributeType.FLOAT,
			value = 0.4,
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
