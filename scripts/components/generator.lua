return {
	name = "Generator",
	description = "Generic generator",
	attributes = {
		type = {
			title = "Type",
			description = "",
			type = AttributeType.STRING,
			value = "Generator",
		},
		recipe = {
			title = "Recipe",
			description = "Active recipe",
			type = AttributeType.STRING,
			value = "Consume Spark Ore",
		},
		load = {
			title = "Load",
			description = "Component load",
			type = AttributeType.FLOAT,
			value = 1.0,
		},
		production = {
			title = "Production",
			description = "Power production",
			type = AttributeType.FLOAT,
			value = 1200.0,
			easing = {
				type = AttributeEasingType.SIN,
				range = 0.5,
				period = 2000.0,
			},
		},
		heat = {
			title = "Active heat",
			description = "Heat generated while active",
			type = AttributeType.FLOAT,
			value = 0.75,
		},
		activation_time = {
			title = "Activation Time",
			description = "Time to activate",
			type = AttributeType.INT,
			value = 2000,
		},
		activation_consumption = {
			title = "Activation Consumption",
			description = "Power consumption during activation",
			type = AttributeType.FLOAT,
			value = 500.0,
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.L,
	api = {},
}
