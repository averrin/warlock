return {
	name = "Solar Panel",
	category = "Power",
	description = "Medium Solar Panel",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			description = "",
			type = AttributeType.STRING,
			value = "Solar",
		},
		production = {
			title = "Production",
			description = "Power production",
			type = AttributeType.FLOAT,
			value = 500.0,
		},
		stable = {
			title = "Stable",
			description = "",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.L,
	spendable_cost = { ["Electronic Parts"] = 50 },
	api = {},
}
