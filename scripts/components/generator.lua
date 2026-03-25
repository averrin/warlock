return {
	name = "Generator",
	category = "Power",
	description = "Spark Stone generator",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Generator",
		},
		recipe = {
			title = "Recipe",
			description = "Active recipe",
			type = AttributeType.STRING,
			value = "Consume Spark Stone",
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
				range = 0.3,
				period = 2000.0,
			},
			inspector = { precision = 1 },
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
