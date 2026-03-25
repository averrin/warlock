return {
	name = "Miner",
	category = "Production",
	description = "Standard miner",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
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
			value = "",
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
			inspector = { precision = 1 },
		},
		debug_produces_deposit = {
			title = "Debug: recipe matches deposit",
			type = AttributeType.BOOL,
			value = false,
		},
		debug_deposit_cells = {
			title = "Debug: frame cells on a deposit",
			type = AttributeType.INT,
			value = 0,
		},
		debug_matched_output = {
			title = "Debug: first recipe output matching deposit",
			type = AttributeType.STRING,
			value = "",
		},
		debug_deposit_items = {
			title = "Debug: patch item names under frame",
			type = AttributeType.STRING,
			value = "",
		},
		debug_suggested_recipe = {
			title = "Debug: first miner recipe that fits deposit",
			type = AttributeType.STRING,
			value = "",
		},
	},
	state = ComponentState.DEACTIVATED,
	size = ComponentSize.M,
	require = { "Power Wire Connector" },
	api = {},
}
