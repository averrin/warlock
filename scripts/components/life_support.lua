return {
	name = "Life Support",
	category = "Unique",
	description = "Life Support",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			value = "Consumer",
		},
		consumption = {
			title = "Consumption",
			description = "Power consumption",
			type = AttributeType.FLOAT,
			value = 500.0,
			easing = {
				type = AttributeEasingType.SIN,
				range = 0.1,
				period = 650.0,
			},
			inspector = { precision = 1 },
		},
		temp_proof = {
			title = "Temp Proof",
			description = "Does not overheat or freeze",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVATED,
	size = ComponentSize.M,
	spendable_cost = { ["Electronic Parts"] = 25 },
	api = {},
}