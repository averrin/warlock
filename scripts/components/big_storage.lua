return {
	name = "Big Storage",
	category = "Storage",
	description = "Big Storage",
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
			value = 4,
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
	api = {},
}
