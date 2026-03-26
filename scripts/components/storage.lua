return {
	name = "Storage",
	category = "Storage",
	description = "Minor Storage",
	icon = "box.png",
	attributes = {
		type = {
			title = "Type",
			inspector = { readonly = true },
			value = "Storage",
		},
		slots = {
			title = "Slots",
			description = "Number of slots",
			type = AttributeType.INT,
			value = 1,
		},
		passive = {
			title = "Passive",
			description = "Passive storage",
			type = AttributeType.BOOL,
			value = true,
		},
	},
	state = ComponentState.ACTIVE,
	size = ComponentSize.M,
	spendable_cost = { ["Electronic Parts"] = 25 },
	api = {},
}
